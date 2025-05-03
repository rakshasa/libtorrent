#include "config.h"

#ifdef USE_KQUEUE

#include "poll.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <unistd.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>

#include "utils/log.h"
#include "utils/thread.h"
#include "torrent/exceptions.h"
#include "torrent/event.h"

// TODO: Add new log category.
#define LT_LOG_EVENT(event, log_level, log_fmt, ...)                    \
  lt_log_print(LOG_SOCKET_##log_level, "kqueue->%s(%i) : " log_fmt, event->type_name(), event->file_descriptor(), __VA_ARGS__);

namespace torrent {

std::function<Poll*()> Poll::m_slot_create_poll;

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
  unsigned int        m_waitingEvents{};
  unsigned int        m_changedEvents{};

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
                      m_changedEvents,
                      m_events.get() + m_waitingEvents,
                      m_max_events - m_waitingEvents,
                      &timeout);

  if (nfds == -1)
    throw internal_error("PollInternal::flush_events() error: " + std::string(std::strerror(errno)));

  m_changedEvents = 0;
  m_waitingEvents += nfds;
}

void
PollInternal::modify(Event* event, unsigned short op, short mask) {
  LT_LOG_EVENT(event, DEBUG, "modify event : op:%hx mask:%hx changed:%u", op, mask, m_changedEvents);

  // Flush the changed filters to the kernel if the buffer is full.
  if (m_changedEvents == m_max_events) {
    if (::kevent(m_fd, m_changes.get(), m_changedEvents, nullptr, 0, nullptr) == -1)
      throw internal_error("PollInternal::modify() error: " + std::string(std::strerror(errno)));

    m_changedEvents = 0;
  }

  struct kevent* itr = m_changes.get() + (m_changedEvents++);

  assert(event == m_table[event->file_descriptor()].second);
  EV_SET(itr, event->file_descriptor(), mask, op, 0, 0, event);
}

// TODO: Use unique_ptr
Poll*
Poll::create(int max_open_sockets) {
  int fd = kqueue();

  if (fd == -1)
    return nullptr;

  auto poll = new Poll();
  poll->m_internal = std::make_unique<PollInternal>();

  poll->m_internal->m_table.resize(max_open_sockets);
  poll->m_internal->m_fd = fd;
  poll->m_internal->m_max_events = 1024;
  poll->m_internal->m_events = std::make_unique<struct kevent[]>(poll->m_internal->m_max_events);
  poll->m_internal->m_changes = std::make_unique<struct kevent[]>(max_open_sockets);

  return poll;
}

Poll::~Poll() {
  m_internal->m_table.clear();

  ::close(m_internal->m_fd);
}

unsigned int
Poll::do_poll(int64_t timeout_usec, int flags) {
  timeout_usec += 10;

  if (!(flags & poll_worker_thread))
    utils::Thread::release_global_lock();

  int status = poll((timeout_usec + 999) / 1000);

  if (!(flags & poll_worker_thread))
    utils::Thread::acquire_global_lock();

  if (status == -1) {
    if (errno != EINTR)
      throw internal_error("Poll::work(): " + std::string(std::strerror(errno)));

    return 0;
  }

  return perform();
}

int
Poll::poll(int msec) {
  timespec timeout = { msec / 1000, (msec % 1000) * 1000000 };

  int nfds = ::kevent(m_internal->m_fd,
                      m_internal->m_changes.get(),
                      m_internal->m_changedEvents,
                      m_internal->m_events.get() + m_internal->m_waitingEvents,
                      m_internal->m_max_events - m_internal->m_waitingEvents,
                      &timeout);

  // Clear the changed events even on fail as we might have received a
  // signal or similar, and the changed events have already been
  // consumed.
  //
  // There's a chance a bad changed event could make kevent return -1,
  // but it won't as long as there is room enough in m_internal->m_events.
  m_internal->m_changedEvents = 0;

  if (nfds == -1)
    return -1;

  // TODO: Different from poll_epoll.
  m_internal->m_waitingEvents += nfds;
  return nfds;
}

unsigned int
Poll::perform() {
  unsigned int count = 0;

  for (struct kevent *itr = m_internal->m_events.get(), *last = m_internal->m_events.get() + m_internal->m_waitingEvents; itr != last; ++itr) {
    if (itr->ident >= m_internal->m_table.size())
      continue;

    if ((flags() & flag_waive_global_lock) && utils::Thread::global_queue_size() != 0)
      utils::Thread::waive_global_lock();

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

  m_internal->m_waitingEvents = 0;
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

  // Shouldn't be needed anymore.
  //
  // TODO: We seem to null it out, so the remove_if below will not do anything.

  for (struct kevent *itr = m_internal->m_events.get(), *last = m_internal->m_events.get() + m_internal->m_waitingEvents; itr != last; ++itr)
    if (itr->udata == event)
      itr->udata = nullptr;

  auto last_itr = std::remove_if(m_internal->m_changes.get(),
                                 m_internal->m_changes.get() + m_internal->m_changedEvents,
                                 [event](const struct kevent& ke) { return ke.udata == event; });

  m_internal->m_changedEvents = last_itr - m_internal->m_changes.get();
}

void
Poll::closed(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "closed event", 0);

  // Kernel removes closed FDs automatically, so just clear the mask
  // and remove it from pending calls.  Don't touch if the FD was
  // re-used before we received the close notification.
  if (m_internal->m_table[event->file_descriptor()].second == event)
    m_internal->m_table[event->file_descriptor()] = PollInternal::Table::value_type();

  // Shouldn't be needed anymore.
  for (struct kevent *itr = m_internal->m_events.get(), *last = m_internal->m_events.get() + m_internal->m_waitingEvents; itr != last; ++itr)
    if (itr->udata == event)
      itr->udata = nullptr;

  auto last_itr = std::remove_if(m_internal->m_changes.get(),
                                 m_internal->m_changes.get() + m_internal->m_changedEvents,
                                 [event](const struct kevent& ke) { return ke.udata == event; });

  m_internal->m_changedEvents = last_itr - m_internal->m_changes.get();
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

}

#endif // USE_KQUEUE
