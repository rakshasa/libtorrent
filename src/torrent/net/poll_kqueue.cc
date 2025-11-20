#include "config.h"

#ifdef USE_KQUEUE

#include "poll.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <map>
#include <unistd.h>
#include <sys/event.h>

#include "utils/log.h"
#include "utils/thread.h"
#include "torrent/exceptions.h"
#include "torrent/event.h"

// TODO: Change to LOG_CONNECTION_POLL

#define LT_LOG_EVENT(log_fmt, ...)                                      \
  lt_log_print(LOG_CONNECTION_FD, "kqueue->%i : %s : " log_fmt, event->file_descriptor(), event->type_name(), __VA_ARGS__);

#define LT_LOG_DEBUG_IDENT(log_fmt, ...)
// #define LT_LOG_DEBUG_IDENT(log_fmt, ...)                                \
//   lt_log_print(LOG_CONNECTION_FD, "kqueue->%u: " log_fmt, static_cast<unsigned int>(itr->ident), __VA_ARGS__);

namespace torrent::net {

class PollEvent {
public:
  PollEvent(Event* e) : event(e) {}
  ~PollEvent() = default;

  uint32_t            mask{};
  Event*              event{};

  // Keep a shared ptr to self to manage lifetime, as changed events returned by kevent might be
  // processed after the event has been removed.
  std::shared_ptr<PollEvent> self_ptr;
};

class PollInternal {
public:
  using Table = std::map<unsigned int, std::shared_ptr<PollEvent>>;

  static constexpr uint32_t flag_read  = 0x1;
  static constexpr uint32_t flag_write = 0x2;
  static constexpr uint32_t flag_error = 0x4;

  inline uint32_t     event_mask(Event* e);
  inline void         set_event_mask(Event* event, uint32_t mask);

  void                modify(torrent::Event* event, unsigned short op, short mask);

  int                 m_fd;

  unsigned int        m_max_sockets{};
  unsigned int        m_max_events{};
  unsigned int        m_waiting_events{};
  unsigned int        m_changed_events{};

  Table                            m_table;
  std::unique_ptr<struct kevent[]> m_events;
  std::unique_ptr<struct kevent[]> m_changes;
};

uint32_t
PollInternal::event_mask(Event* event) {
  if (event->file_descriptor() == -1)
    throw internal_error("PollInternal::event_mask() invalid file descriptor for event: " + event->print_name_fd_str());

  auto itr = m_table.find(event->file_descriptor());

  if (itr == m_table.end())
    throw internal_error("PollInternal::event_mask() event not found: " + event->print_name_fd_str());

  if (event != itr->second->event)
    throw internal_error("PollInternal::event_mask() event mismatch: " + event->print_name_fd_str());

  return itr->second->mask;
}

void
PollInternal::set_event_mask(Event* event, uint32_t mask) {
  if (event->file_descriptor() == -1)
    throw internal_error("PollInternal::set_event_mask() invalid file descriptor for event: " + event->print_name_fd_str());

  event->m_poll_event->mask = mask;
}

void
PollInternal::modify(Event* event, unsigned short op, short mask) {
  LT_LOG_EVENT("modify event : op:%hx mask:%hx changed:%u", op, mask, m_changed_events);

  if (m_changed_events == m_max_events) {
    if (::kevent(m_fd, m_changes.get(), m_changed_events, nullptr, 0, nullptr) == -1)
      throw internal_error("PollInternal::modify() error: " + std::string(std::strerror(errno)));

    m_changed_events = 0;
  }

  if (event->m_poll_event == nullptr)
    throw internal_error("PollInternal::modify() event has no poll event: " + event->print_name_fd_str());

  struct kevent* itr = m_changes.get() + (m_changed_events++);

  EV_SET(itr, event->file_descriptor(), mask, op, 0, 0, event->m_poll_event.get());
}

std::unique_ptr<Poll>
Poll::create() {
  auto socket_open_max = sysconf(_SC_OPEN_MAX);

  if (socket_open_max == -1)
    throw internal_error("Poll::create() : sysconf(_SC_OPEN_MAX) failed : " + std::string(std::strerror(errno)));

  int fd = ::kqueue();

  if (fd == -1)
    throw internal_error("Poll::create() : kqueue() failed : " + std::string(std::strerror(errno)));

  auto poll = new Poll();

  poll->m_internal                = std::make_unique<PollInternal>();
  poll->m_internal->m_fd          = fd;
  poll->m_internal->m_max_sockets = static_cast<unsigned int>(socket_open_max);
  poll->m_internal->m_max_events  = 1024;
  poll->m_internal->m_events      = std::make_unique<struct kevent[]>(poll->m_internal->m_max_events);
  poll->m_internal->m_changes     = std::make_unique<struct kevent[]>(poll->m_internal->m_max_events);

  return std::unique_ptr<Poll>(poll);
}

Poll::~Poll() {
  assert(m_internal->m_table.empty() && "Poll::~Poll() called with non-empty event table.");

  m_internal->m_table.clear();

  ::close(m_internal->m_fd);
  m_internal->m_fd = -1;
}

unsigned int
Poll::do_poll(int64_t timeout_usec) {
  int status = poll(timeout_usec);

  if (status == -1) {
    if (errno != EINTR)
      throw internal_error("Poll::do_poll() error: " + std::string(std::strerror(errno)));

    return 0;
  }

  return process();
}

int
Poll::poll(int timeout_usec) {
  timespec timeout = { timeout_usec / 1000000, (timeout_usec % 1000000) * 1000 };

  int nfds = ::kevent(m_internal->m_fd,
                      m_internal->m_changes.get(),
                      m_internal->m_changed_events,
                      m_internal->m_events.get(),
                      m_internal->m_max_events,
                      &timeout);

  // Clear the changed events even on fail as we might have received a
  // signal or similar, and the changed events have already been
  // consumed.
  //
  // There's a chance a bad changed event could make kevent return -1,
  // but it won't as long as there is room enough in m_internal->m_events.
  m_internal->m_changed_events = 0;

  if (nfds == -1)
    return -1;

  m_internal->m_waiting_events = nfds;
  return nfds;
}

unsigned int
Poll::process() {
  unsigned int count = 0;

  for (struct kevent *itr = m_internal->m_events.get(), *last = m_internal->m_events.get() + m_internal->m_waiting_events; itr != last; ++itr) {
    if (utils::Thread::self()->callbacks_should_interrupt_polling())
      utils::Thread::self()->process_callbacks(true);

    auto* poll_event = static_cast<PollEvent*>(itr->udata);
    auto* event = poll_event->event;

    if (event == nullptr) {
      LT_LOG_DEBUG_IDENT("event is null, skipping : udata:%p", itr->udata);
      continue;
    }

    if ((itr->flags & EV_ERROR)) {
      if (!(poll_event->mask & PollInternal::flag_error))
        throw internal_error("Poll::process() received error event for event not in error: " + poll_event->event->print_name_fd_str());

      auto event_info = event->print_name_fd_str();

      event->event_error();

      if (poll_event->mask != 0)
        throw internal_error("Poll::process() event_error called but event mask not cleared: " + event_info);

      count++;
      continue;
    }

    if (itr->filter == EVFILT_READ && (poll_event->mask & PollInternal::flag_read)) {
      count++;
      event->event_read();
    }
    else if (itr->filter == EVFILT_READ) {
      LT_LOG_DEBUG_IDENT("spurious read event, skipping", 0);
    }

    if (itr->filter == EVFILT_WRITE && (poll_event->mask & PollInternal::flag_write)) {
      count++;
      event->event_write();
    }
    else if (itr->filter == EVFILT_WRITE) {
      LT_LOG_DEBUG_IDENT("spurious write event, skipping", 0);
    }

  }

  m_internal->m_waiting_events = 0;
  return count;
}

uint32_t
Poll::open_max() const {
  return m_internal->m_max_sockets;
}

void
Poll::open(Event* event) {
  LT_LOG_EVENT("open event", 0);

  if (event->file_descriptor() == -1)
    throw internal_error("PollInternal::event_mask_open() invalid file descriptor for event: name:" + std::string(event->type_name()));

  if (m_internal->m_table.find(event->file_descriptor()) != m_internal->m_table.end())
    throw internal_error("PollInternal::event_mask_open() event already exists: " + event->print_name_fd_str());

  event->m_poll_event = std::make_shared<PollEvent>(event);
  event->m_poll_event->self_ptr = event->m_poll_event;

  m_internal->m_table[event->file_descriptor()] = event->m_poll_event;
}

void
Poll::close(Event* event) {
  LT_LOG_EVENT("close event", 0);

  auto* poll_event = event->m_poll_event.get();

  if (poll_event == nullptr)
    return;

  if (poll_event->event != event)
    throw internal_error("Poll::close() event mismatch: " + event->print_name_fd_str());

  if (m_internal->event_mask(event) != 0)
    throw internal_error("Poll::close() called but the file descriptor is active: " + event->print_name_fd_str());

  if (m_internal->m_table.erase(event->file_descriptor()) == 0)
    throw internal_error("Poll::close() event not found: " + event->print_name_fd_str());

  // No need to touch m_events as we unset the read/write/error flags in m_internal->m_events using
  // remove_read/write/error.
  auto last_itr = std::remove_if(m_internal->m_changes.get(),
                                 m_internal->m_changes.get() + m_internal->m_changed_events,
                                 [poll_event](const struct kevent& ke) { return ke.udata == poll_event; });

  m_internal->m_changed_events = last_itr - m_internal->m_changes.get();

  poll_event->event = nullptr;
  poll_event->self_ptr.reset();

  event->m_poll_event.reset();
}

bool
Poll::in_read(Event* event) {
  return m_internal->event_mask(event) & PollInternal::flag_read;
}

bool
Poll::in_write(Event* event) {
  return m_internal->event_mask(event) & PollInternal::flag_write;
}

bool
Poll::in_error(Event* event) {
  return m_internal->event_mask(event) & PollInternal::flag_error;
}

void
Poll::insert_read(Event* event) {
  auto event_mask = m_internal->event_mask(event);

  if (event_mask & PollInternal::flag_read) {
    LT_LOG_EVENT("insert read: already in read", 0);
    return;
  }

  LT_LOG_EVENT("insert read", 0);

  m_internal->set_event_mask(event, event_mask | PollInternal::flag_read);
  m_internal->modify(event, EV_ADD, EVFILT_READ);
}

void
Poll::insert_write(Event* event) {
  auto event_mask = m_internal->event_mask(event);

  if (event_mask & PollInternal::flag_write) {
    LT_LOG_EVENT("insert write: already in write", 0);
    return;
  }

  LT_LOG_EVENT("insert write", 0);

  m_internal->set_event_mask(event, event_mask | PollInternal::flag_write);
  m_internal->modify(event, EV_ADD, EVFILT_WRITE);
}

void
Poll::insert_error(Event* event) {
  LT_LOG_EVENT("insert error", 0);
}

void
Poll::remove_read(Event* event) {
  auto event_mask = m_internal->event_mask(event);

  if (!(event_mask & PollInternal::flag_read)) {
    LT_LOG_EVENT("remove read: not in read", 0);
    return;
  }

  LT_LOG_EVENT("remove read", 0);

  m_internal->set_event_mask(event, event_mask & ~PollInternal::flag_read);
  m_internal->modify(event, EV_DELETE, EVFILT_READ);
}

void
Poll::remove_write(Event* event) {
  auto event_mask = m_internal->event_mask(event);

  if (!(event_mask & PollInternal::flag_write)) {
    LT_LOG_EVENT("remove write: not in write", 0);
    return;
  }

  LT_LOG_EVENT("remove write", 0);

  m_internal->set_event_mask(event, event_mask & ~PollInternal::flag_write);
  m_internal->modify(event, EV_DELETE, EVFILT_WRITE);
}

void
Poll::remove_error(Event* event) {
  LT_LOG_EVENT("remove error", 0);
}

void
Poll::remove_and_close(Event* event) {
  LT_LOG_EVENT("remove and close", 0);

  remove_read(event);
  remove_write(event);
  remove_error(event);

  close(event);
}

}

#endif // USE_KQUEUE
