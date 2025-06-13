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

// TODO: Add new log category.
#define LT_LOG_EVENT(event, log_level, log_fmt, ...)                                                                              \
  do {                                                                                                                            \
    lt_log_print(LOG_SOCKET_##log_level, "kqueue->%s(%i) : " log_fmt, event->type_name(), event->file_descriptor(), __VA_ARGS__); \
  } while (false)

namespace torrent {

class PollInternal {
public:
  using Table = std::vector<std::pair<uint32_t, Event*>>;

  static constexpr uint32_t flag_read  = (1 << 0);
  static constexpr uint32_t flag_write = (1 << 1);
  static constexpr uint32_t flag_error = (1 << 2);

  inline uint32_t     event_mask(Event* e);
  inline void         set_event_mask(Event* e, uint32_t m);

  void                flush_events();
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
  assert(e->file_descriptor() != -1);

  Table::value_type entry = m_table[e->file_descriptor()];
  return entry.second != e ? 0 : entry.first;
}

inline void
PollInternal::set_event_mask(Event* e, uint32_t m) {
  assert(e->file_descriptor() != -1);

  m_table[e->file_descriptor()] = Table::value_type(m, e);
}

void
PollInternal::flush_events() {
  timespec timeout = { 0, 0 };

  int nfds = ::kevent(m_fd,
                      m_changes.get(),
                      m_changed_events,
                      m_events.get() + m_waiting_events,
                      m_max_events - m_waiting_events,
                      &timeout);

  if (nfds == -1)
    throw internal_error("PollInternal::flush_events() error: " + std::string(std::strerror(errno)));

  m_changed_events = 0;
  m_waiting_events += nfds;
}

void
PollInternal::modify(Event* event, unsigned short op, short mask) {
  LT_LOG_EVENT(event, DEBUG, "modify event : op:%hx mask:%hx changed:%u", op, mask, m_changed_events);

  // Flush the changed filters to the kernel if the buffer is full.
  if (m_changed_events == m_max_events) {
    if (::kevent(m_fd, m_changes.get(), m_changed_events, nullptr, 0, nullptr) == -1)
      throw internal_error("PollInternal::modify() error: " + std::string(std::strerror(errno)));

    m_changed_events = 0;
  }

  struct kevent* itr = m_changes.get() + (m_changed_events++);

  assert(event == m_table[event->file_descriptor()].second);
  EV_SET(itr, event->file_descriptor(), mask, op, 0, 0, event);
}

std::unique_ptr<Poll>
Poll::create() {
  auto socket_open_max = sysconf(_SC_OPEN_MAX);

  if (socket_open_max == -1)
    throw internal_error("Poll::create(): sysconf(_SC_OPEN_MAX) failed: " + std::string(std::strerror(errno)));

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
      throw internal_error("Poll::work(): " + std::string(std::strerror(errno)));

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
                      m_internal->m_events.get() + m_internal->m_waiting_events,
                      m_internal->m_max_events - m_internal->m_waiting_events,
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

  m_internal->m_waiting_events += nfds;
  return nfds;
}

unsigned int
Poll::process() {
  unsigned int count = 0;

  for (struct kevent *itr = m_internal->m_events.get(), *last = m_internal->m_events.get() + m_internal->m_waiting_events; itr != last; ++itr) {
    if (itr->ident >= m_internal->m_table.size())
      continue;

    if (utils::Thread::self()->callbacks_should_interrupt_polling())
      utils::Thread::self()->process_callbacks(true);

    auto evItr = m_internal->m_table.begin() + itr->ident;

    if ((itr->flags & EV_ERROR) && evItr->second != nullptr) {
      if (evItr->first & PollInternal::flag_error)
        evItr->second->event_error();

      count++;

      // We assume that the event gets closed if we get an error.
      continue;
    }

    // Also check current mask.

    if (itr->filter == EVFILT_READ && evItr->second != nullptr && evItr->first & PollInternal::flag_read) {
      count++;
      evItr->second->event_read();
    }

    if (itr->filter == EVFILT_WRITE && evItr->second != nullptr && evItr->first & PollInternal::flag_write) {
      count++;
      evItr->second->event_write();
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
  LT_LOG_EVENT(event, DEBUG, "open event", 0);

  if (m_internal->event_mask(event) != 0)
    throw internal_error("Poll::open(...) called but the file descriptor is active");
}

void
Poll::close(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "close event", 0);

  if (m_internal->event_mask(event) != 0)
    throw internal_error("Poll::close(...) called but the file descriptor is active");

  m_internal->m_table[event->file_descriptor()] = PollInternal::Table::value_type();

  // No need to touch m_events as we unset the read/write/error flags in m_internal->m_events using
  // remove_read/write/error.
  auto last_itr = std::remove_if(m_internal->m_changes.get(),
                                 m_internal->m_changes.get() + m_internal->m_changed_events,
                                 [event](const struct kevent& ke) { return ke.udata == event; });

  m_internal->m_changed_events = last_itr - m_internal->m_changes.get();

  // Clear the event list just in case we open a new socket with the
  // same fd while in the middle of calling Poll::perform.
  //
  // Removed.
  //
  // Shouldn't be needed as we unset the read/write/error flags in m_internal->m_events using
  // remove_read/write/error.
}

void
Poll::cleanup_closed(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "cleanup_closed event", 0);

  if (m_internal->event_mask(event) != 0)
    throw internal_error("Poll::cleanup_closed(...) called but the file descriptor is active");

  // Kernel removes closed FDs automatically, so just clear the mask
  // and remove it from pending calls.  Don't touch if the FD was
  // re-used before we received the close notification.
  if (m_internal->m_table[event->file_descriptor()].second == event)
    m_internal->m_table[event->file_descriptor()] = PollInternal::Table::value_type();

  auto last_itr = std::remove_if(m_internal->m_changes.get(),
                                 m_internal->m_changes.get() + m_internal->m_changed_events,
                                 [event](const struct kevent& ke) { return ke.udata == event; });

  m_internal->m_changed_events = last_itr - m_internal->m_changes.get();

  // Clear the event list just in case we open a new socket with the
  // same fd while in the middle of calling Poll::perform.
  //
  // Removed.
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
  if (m_internal->event_mask(event) & PollInternal::flag_read)
    return;

  LT_LOG_EVENT(event, DEBUG, "insert read", 0);

  m_internal->set_event_mask(event, m_internal->event_mask(event) | PollInternal::flag_read);
  m_internal->modify(event, EV_ADD, EVFILT_READ);
}

void
Poll::insert_write(Event* event) {
  if (m_internal->event_mask(event) & PollInternal::flag_write)
    return;

  LT_LOG_EVENT(event, DEBUG, "insert write", 0);

  m_internal->set_event_mask(event, m_internal->event_mask(event) | PollInternal::flag_write);
  m_internal->modify(event, EV_ADD, EVFILT_WRITE);
}

void
Poll::insert_error(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "insert error", 0);
}

void
Poll::remove_read(Event* event) {
  if (!(m_internal->event_mask(event) & PollInternal::flag_read))
    return;

  LT_LOG_EVENT(event, DEBUG, "remove read", 0);

  m_internal->set_event_mask(event, m_internal->event_mask(event) & ~PollInternal::flag_read);
  m_internal->modify(event, EV_DELETE, EVFILT_READ);
}

void
Poll::remove_write(Event* event) {
  if (!(m_internal->event_mask(event) & PollInternal::flag_write))
    return;

  LT_LOG_EVENT(event, DEBUG, "remove write", 0);

  m_internal->set_event_mask(event, m_internal->event_mask(event) & ~PollInternal::flag_write);
  m_internal->modify(event, EV_DELETE, EVFILT_WRITE);
}

void
Poll::remove_error(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "remove error", 0);
}

void
Poll::remove_and_close(Event* event) {
  remove_read(event);
  remove_write(event);
  // remove_error(event);

  close(event);
}

void
Poll::remove_and_cleanup_closed(Event* event) {
  remove_read(event);
  remove_write(event);
  // remove_error(event);

  cleanup_closed(event);
}

}

#endif // USE_KQUEUE
