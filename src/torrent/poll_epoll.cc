#include "config.h"

#ifdef USE_EPOLL

#include "torrent/poll.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/types.h>

#include "torrent/exceptions.h"
#include "torrent/event.h"
#include "torrent/utils/log.h"
#include "torrent/utils/thread.h"

#define LT_LOG_EVENT(event, log_level, log_fmt, ...)                    \
  lt_log_print(LOG_SOCKET_##log_level, "epoll->%s(%i): " log_fmt, event->type_name(), event->file_descriptor(), __VA_ARGS__);

namespace torrent {

std::function<Poll*()> Poll::m_slot_create_poll;

class PollInternal {
public:
  using Table = std::vector<std::pair<uint32_t, Event*>>;

  inline uint32_t     event_mask(Event* e);
  inline void         set_event_mask(Event* e, uint32_t m);

  void                flush_events() LIBTORRENT_NO_EXPORT;
  void                modify(torrent::Event* event, unsigned short op, uint32_t mask);

  int                 m_fd;

  unsigned int        m_maxEvents;
  unsigned int        m_waitingEvents{};

  Table                                 m_table;
  std::unique_ptr<struct epoll_event[]> m_events;
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
PollInternal::modify(Event* event, unsigned short op, uint32_t mask) {
  if (event_mask(event) == mask)
    return;

  LT_LOG_EVENT(event, DEBUG, "modify event : op:%hx mask:%hx", op, mask);

  epoll_event e;
  e.data.u64 = 0;
  e.data.fd = event->file_descriptor();
  e.events = mask;

  set_event_mask(event, mask);

  if (epoll_ctl(m_fd, op, event->file_descriptor(), &e)) {
    // Socket was probably already closed. Ignore this.
    if (op == EPOLL_CTL_DEL && errno == ENOENT)
      return;

    // Handle some libcurl/c-ares bugs by retrying once.
    int retry = op;

    if (op == EPOLL_CTL_ADD && errno == EEXIST) {
      retry = EPOLL_CTL_MOD;
      errno = 0;
    } else if (op == EPOLL_CTL_MOD && errno == ENOENT) {
      retry = EPOLL_CTL_ADD;
      errno = 0;
    }

    if (errno || epoll_ctl(m_fd, retry, event->file_descriptor(), &e)) {
      char errmsg[1024];
      snprintf(errmsg, sizeof(errmsg),
               "Poll::modify(...) epoll_ctl(%d, %d -> %d, %d, [%p:%x]) = %d: %s",
               m_fd, op, retry, event->file_descriptor(), event, mask, errno, strerror(errno));

      throw internal_error(errmsg);
    }
  }
}

// TODO: Use unique_ptr
Poll*
Poll::create(int max_open_sockets) {
  int fd = epoll_create(max_open_sockets);

  if (fd == -1)
    return nullptr;

  auto poll = new Poll();
  poll->m_internal = std::make_unique<PollInternal>();

  poll->m_internal->m_table.resize(max_open_sockets);
  poll->m_internal->m_fd = fd;
  poll->m_internal->m_maxEvents = 1024;
  poll->m_internal->m_events = std::make_unique<struct epoll_event[]>(poll->m_internal->m_maxEvents);

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
  int nfds = epoll_wait(m_internal->m_fd, m_internal->m_events, m_internal->m_maxEvents, msec);

  if (nfds == -1)
    return -1;

  m_internal->m_waitingEvents = nfds;
  return nfds;
}

// We check m_internal->m_table to make sure the Event is still listening to the
// event, so it is safe to remove Event's while in working.
//
// TODO: Do we want to guarantee if the Event has been removed from
// some event but not closed, it won't call that event? Think so...
unsigned int
Poll::perform() {
  unsigned int count = 0;

  for (epoll_event *itr = m_internal->m_events.get(), *last = m_internal->m_events.get() + m_internal->m_waitingEvents; itr != last; ++itr) {
    // TODO: These should be asserts?
    if (itr->data.fd < 0 || static_cast<size_t>(itr->data.fd) >= m_internal->m_table.size())
      continue;

    if ((flags() & flag_waive_global_lock) && utils::Thread::global_queue_size() != 0)
      utils::Thread::waive_global_lock();

    auto evItr = m_internal->m_table.begin() + itr->data.fd;

    // Each branch must check for data.ptr != nullptr to allow the socket
    // to remove itself between the calls.
    //
    // TODO: Make it so that it checks that read/write is wanted, that
    // it wasn't removed from one of them but not closed.

    if (itr->events & EPOLLERR && evItr->second != nullptr && evItr->first & EPOLLERR) {
      count++;
      evItr->second->event_error();
    }

    if (itr->events & EPOLLIN && evItr->second != nullptr && evItr->first & EPOLLIN) {
      count++;
      evItr->second->event_read();
    }

    if (itr->events & EPOLLOUT && evItr->second != nullptr && evItr->first & EPOLLOUT) {
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

  if (event_mask(event) != 0)
    throw internal_error("Poll::open(...) called but the file descriptor is active");
}

void
Poll::close(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "close event", 0);

  if (event_mask(event) != 0)
    throw internal_error("Poll::close(...) called but the file descriptor is active");

  m_internal->m_table[event->file_descriptor()] = Table::value_type();

  // Clear the event list just in case we open a new socket with the
  // same fd while in the middle of calling Poll::perform.
  for (epoll_event *itr = m_internal->m_events.get(), *last = m_internal->m_events.get() + m_internal->m_waitingEvents; itr != last; ++itr)
    if (itr->data.fd == event->file_descriptor())
      itr->events = 0;
}

void
Poll::closed(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "closed event", 0);

  // Kernel removes closed FDs automatically, so just clear the mask and remove it from pending calls.
  // Don't touch if the FD was re-used before we received the close notification.
  if (m_internal->m_table[event->file_descriptor()].second == event)
    m_internal->m_table[event->file_descriptor()] = Table::value_type();

  // for (epoll_event *itr = m_internal->m_events.get(), *last = m_internal->m_events.get() + m_internal->m_waitingEvents; itr != last; ++itr) {
  //   if (itr->data.fd == event->file_descriptor())
  //     itr->events = 0;
  // }
}

bool
Poll::in_read(Event* event) {
  return m_internal->event_mask(event) & EPOLLIN;
}

bool
Poll::in_write(Event* event) {
  return m_internal->event_mask(event) & EPOLLOUT;
}

bool
Poll::in_error(Event* event) {
  return m_internal->event_mask(event) & EPOLLERR;
}

void
Poll::insert_read(Event* event) {
  if (m_internal->event_mask(event) & EPOLLIN)
    return;

  LT_LOG_EVENT(event, DEBUG, "insert read", 0);

  modify(event,
         event_mask(event) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
         event_mask(event) | EPOLLIN);
}

void
Poll::insert_write(Event* event) {
  if (m_internal->event_mask(event) & EPOLLOUT)
    return;

  LT_LOG_EVENT(event, DEBUG, "insert write", 0);

  modify(event,
         event_mask(event) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
         event_mask(event) | EPOLLOUT);
}

void
Poll::insert_error(Event* event) {
  if (m_internal->event_mask(event) & EPOLLERR)
    return;

  LT_LOG_EVENT(event, DEBUG, "insert error", 0);

  modify(event,
	 event_mask(event) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
	 event_mask(event) | EPOLLERR);
}

void
Poll::remove_read(Event* event) {
  if (!(m_internal->event_mask(event) & EPOLLIN))
    return;

  LT_LOG_EVENT(event, DEBUG, "remove read", 0);

  uint32_t mask = event_mask(event) & ~EPOLLIN;
  modify(event,
         mask ? EPOLL_CTL_MOD : EPOLL_CTL_DEL,
         mask);
}

void
Poll::remove_write(Event* event) {
  if (!(m_internal->event_mask(event) & EPOLLOUT))
    return;

  LT_LOG_EVENT(event, DEBUG, "remove write", 0);

  uint32_t mask = event_mask(event) & ~EPOLLOUT;
  modify(event,
         mask ? EPOLL_CTL_MOD : EPOLL_CTL_DEL,
         mask);
}

void
Poll::remove_error(Event* event) {
  if (!(m_internal->event_mask(event) & EPOLLERR))
    return;

  LT_LOG_EVENT(event, DEBUG, "remove error", 0);

  uint32_t mask = event_mask(event) & ~EPOLLERR;
  modify(event,
         mask ? EPOLL_CTL_MOD : EPOLL_CTL_DEL,
         mask);
}

}

#endif // USE_EPOLL
