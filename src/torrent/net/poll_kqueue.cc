#include "config.h"

#ifdef USE_KQUEUE

#include "poll.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <sys/event.h>
#include <unistd.h>

#include "utils/log.h"
#include "utils/thread.h"
#include "torrent/exceptions.h"
#include "torrent/event.h"

// TODO: Change to LOG_CONNECTION_POLL

// TODO: Optimize table memory size, and add a reference to Event for direct lookup.

#define LT_LOG(log_fmt, ...)                                        \
  lt_log_print(LOG_CONNECTION_FD, "kqueue: " log_fmt, __VA_ARGS__);

#define LT_LOG_EVENT(log_fmt, ...)                                      \
  lt_log_print(LOG_CONNECTION_FD, "kqueue->%i : %s : " log_fmt, event->file_descriptor(), event->type_name(), __VA_ARGS__);

#define LT_LOG_DEBUG_IDENT(log_fmt, ...)
// #define LT_LOG_DEBUG_IDENT(log_fmt, ...)                                \
//   lt_log_print(LOG_CONNECTION_FD, "kqueue->%u: " log_fmt, static_cast<unsigned int>(itr->ident), __VA_ARGS__);

namespace torrent::net {

class PollInternal {
public:
  using Table = std::vector<std::pair<uint32_t, Event*>>;

  static constexpr uint32_t flag_read  = (1 << 0);
  static constexpr uint32_t flag_write = (1 << 1);
  static constexpr uint32_t flag_error = (1 << 2);

  inline uint32_t     event_mask(Event* e);
  inline uint32_t     event_mask_any(int fd);
  inline void         set_event_mask(Event* e, uint32_t m);

  void                flush();
  void                modify(torrent::Event* event, unsigned short op, short mask);

  int                 m_fd;

  unsigned int        m_max_events;
  unsigned int        m_waiting_events{};
  unsigned int        m_changed_events{};

  Table                            m_table;
  std::unique_ptr<struct kevent[]> m_events;
  std::unique_ptr<struct kevent[]> m_changes;
};

inline uint32_t
PollInternal::event_mask(Event* e) {
  if (e->file_descriptor() == -1)
    throw internal_error("PollInternal::event_mask() invalid file descriptor for event: name:" + std::string(e->type_name()));

  if (static_cast<unsigned int>(e->file_descriptor()) >= m_table.size())
    throw internal_error("PollInternal::event_mask() file descriptor out of range: name:" + std::string(e->type_name()) + " fd:" + std::to_string(e->file_descriptor()));

  if (e != m_table[e->file_descriptor()].second)
    throw internal_error("PollInternal::event_mask() event mismatch: name:" + std::string(e->type_name()) + " fd:" + std::to_string(e->file_descriptor()));

  return m_table[e->file_descriptor()].first;
}

inline uint32_t
PollInternal::event_mask_any(int fd) {
  if (fd == -1)
    throw internal_error("PollInternal::event_mask_any() invalid file descriptor for event");

  if (static_cast<unsigned int>(fd) >= m_table.size())
    throw internal_error("PollInternal::event_mask_any() file descriptor out of range: fd:" + std::to_string(fd));

  return m_table[fd].first;
}

inline void
PollInternal::set_event_mask(Event* e, uint32_t m) {
  if (e->file_descriptor() == -1)
    throw internal_error("PollInternal::set_event_mask() invalid file descriptor for event: name:" + std::string(e->type_name()));

  if (static_cast<unsigned int>(e->file_descriptor()) >= m_table.size())
    throw internal_error("PollInternal::set_event_mask() file descriptor out of range: name:" + std::string(e->type_name()) + " fd:" + std::to_string(e->file_descriptor()));

  if (e != m_table[e->file_descriptor()].second)
    throw internal_error("PollInternal::set_event_mask() event mismatch: name:" + std::string(e->type_name()) + " fd:" + std::to_string(e->file_descriptor()));

  m_table[e->file_descriptor()] = Table::value_type(m, e);
}

void
PollInternal::flush() {
  if (m_changed_events == 0)
    return;

  LT_LOG("flushing events : changed:%u", m_changed_events);

  if (::kevent(m_fd, m_changes.get(), m_changed_events, nullptr, 0, nullptr) == -1)
    throw internal_error("PollInternal::flush() error: " + std::string(std::strerror(errno)));

  m_changed_events = 0;
}

void
PollInternal::modify(Event* event, unsigned short op, short mask) {
  LT_LOG_EVENT("modify event : op:%hx mask:%hx changed:%u", op, mask, m_changed_events);

  // Flush the changed filters to the kernel if the buffer is full.
  if (m_changed_events == m_max_events)
    flush();

  struct kevent* itr = m_changes.get() + (m_changed_events++);

  assert(event == m_table[event->file_descriptor()].second);
  EV_SET(itr, event->file_descriptor(), mask, op, 0, 0, event);
}

std::unique_ptr<Poll>
Poll::create() {
  auto socket_open_max = sysconf(_SC_OPEN_MAX);

  if (socket_open_max == -1)
    throw internal_error("Poll::create() : sysconf(_SC_OPEN_MAX) failed: " + std::string(std::strerror(errno)));

  int fd = kqueue();

  if (fd == -1)
    return nullptr;

  auto poll = new Poll();

  poll->m_internal = std::make_unique<PollInternal>();
  poll->m_internal->m_table.resize(socket_open_max);
  poll->m_internal->m_fd = fd;
  poll->m_internal->m_max_events = 1024;
  poll->m_internal->m_events = std::make_unique<struct kevent[]>(poll->m_internal->m_max_events);

  // TODO: Dynamically resize.
  // !!!!! check if correct size
  poll->m_internal->m_changes = std::make_unique<struct kevent[]>(socket_open_max);

  return std::unique_ptr<Poll>(poll);
}

Poll::~Poll() {
  m_internal->m_table.clear();

  ::close(m_internal->m_fd);
}

unsigned int
Poll::do_poll(int64_t timeout_usec) {
  int status = poll(timeout_usec);

  if (status == -1) {
    if (errno != EINTR)
      throw internal_error("Poll::work() " + std::string(std::strerror(errno)));

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
    if (itr->ident >= m_internal->m_table.size())
      throw internal_error("Poll::process() received ident out of range: " + std::to_string(itr->ident));

    if (utils::Thread::self()->callbacks_should_interrupt_polling())
      utils::Thread::self()->process_callbacks(true);

    auto ev_itr = m_internal->m_table.begin() + itr->ident;

    if (ev_itr->second == nullptr) {
      LT_LOG_DEBUG_IDENT("event is null, skipping : flags:%hx fflag:%hx filter:%hx", itr->flags, itr->fflags, itr->filter);
      continue;
    }

    if ((itr->flags & EV_ERROR)) {
      if (ev_itr->first & PollInternal::flag_error)
        ev_itr->second->event_error();

      count++;

      // We assume that the event gets closed if we get an error.
      continue;
    }

    // Also check current mask.

    if (itr->filter == EVFILT_READ && ev_itr->first & PollInternal::flag_read) {
      count++;
      ev_itr->second->event_read();
    }
    else if (itr->filter == EVFILT_READ) {
      LT_LOG_DEBUG_IDENT("spurious read event, skipping", 0);
    }

    if (itr->filter == EVFILT_WRITE && ev_itr->first & PollInternal::flag_write) {
      count++;
      ev_itr->second->event_write();
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
  return m_internal->m_table.size();
}

void
Poll::open(Event* event) {
  LT_LOG_EVENT("open event", 0);

  if (m_internal->event_mask_any(event->file_descriptor()) != 0)
    throw internal_error("Poll::open() called but the file descriptor is active");

  m_internal->m_table[event->file_descriptor()] = PollInternal::Table::value_type(0, event);
}

void
Poll::close(Event* event) {
  LT_LOG_EVENT("close event", 0);

  if (m_internal->event_mask(event) != 0)
    throw internal_error("Poll::close() called but the file descriptor is active");

  m_internal->m_table[event->file_descriptor()] = PollInternal::Table::value_type();

  m_internal->flush();
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
  // remove_error(event);

  close(event);
}

}

#endif // USE_KQUEUE
