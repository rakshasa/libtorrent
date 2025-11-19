#include "config.h"

#ifdef USE_EPOLL

#include "torrent/net/poll.h"

#include <cassert>
#include <cerrno>
#include <sys/epoll.h>
#include <unistd.h>

#include "torrent/exceptions.h"
#include "torrent/event.h"
#include "torrent/utils/log.h"
#include "torrent/utils/thread.h"

#define LT_LOG_EVENT(log_fmt, ...)                                      \
  lt_log_print(LOG_CONNECTION_FD, "epoll->%i : %s : " log_fmt, event->file_descriptor(), event->type_name(), __VA_ARGS__);

#define LT_LOG_DEBUG_DATA_FD(log_fmt, ...)
// #define LT_LOG_DEBUG_DATA_FD(log_fmt, ...)                                \
//   lt_log_print(LOG_CONNECTION_FD, "epoll->%i: " log_fmt, itr->data.fd, __VA_ARGS__);

namespace torrent::net {

class PollInternal {
public:
  using Table = std::vector<std::pair<uint32_t, Event*>>;

  inline uint32_t     event_mask(Event* e);
  inline uint32_t     event_mask_any(int fd);
  inline void         set_event_mask(Event* e, uint32_t m);

  void                modify(torrent::Event* event, unsigned short op, uint32_t mask);

  int                 m_fd;

  unsigned int        m_max_events;
  unsigned int        m_waiting_events{};

  Table                                 m_table;
  std::unique_ptr<struct epoll_event[]> m_events;
};

inline uint32_t
PollInternal::event_mask(Event* e) {
  if (e->file_descriptor() == -1)
    throw internal_error("PollInternal::event_mask() invalid file descriptor for event: name:" + std::string(e->type_name()));

  if (static_cast<unsigned int>(e->file_descriptor()) >= m_table.size())
    throw internal_error("PollInternal::event_mask() file descriptor out of range: name:" + std::string(e->type_name()) + " fd:" + std::to_string(e->file_descriptor()));

  if (m_table[e->file_descriptor()].second != e)
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

  if (m_table[e->file_descriptor()].second != e)
    throw internal_error("PollInternal::set_event_mask() event mismatch: name:" + std::string(e->type_name()) + " fd:" + std::to_string(e->file_descriptor()));

  m_table[e->file_descriptor()] = Table::value_type(m, e);
}

void
PollInternal::modify(Event* event, unsigned short op, uint32_t mask) {
  if (event_mask(event) == mask)
    return;

  LT_LOG_EVENT("modify event : op:%hx mask:%hx", op, mask);

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

std::unique_ptr<Poll>
Poll::create() {
  auto socket_open_max = sysconf(_SC_OPEN_MAX);

  if (socket_open_max == -1)
    throw internal_error("Poll::create() : sysconf(_SC_OPEN_MAX) failed: " + std::string(std::strerror(errno)));

  int fd = epoll_create(socket_open_max);

  if (fd == -1)
    return nullptr;

  auto poll = new Poll();

  poll->m_internal = std::make_unique<PollInternal>();
  poll->m_internal->m_table.resize(socket_open_max);
  poll->m_internal->m_fd = fd;
  poll->m_internal->m_max_events = 1024;
  poll->m_internal->m_events = std::make_unique<struct epoll_event[]>(poll->m_internal->m_max_events);

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
  int nfds = ::epoll_wait(m_internal->m_fd,
                          m_internal->m_events.get(),
                          m_internal->m_max_events,
                          timeout_usec / 1000);

  if (nfds == -1)
    return -1;

  m_internal->m_waiting_events = nfds;
  return nfds;
}

// We check m_internal->m_table to make sure the Event is still listening to the
// event, so it is safe to remove Event's while in working.
//
// TODO: Do we want to guarantee if the Event has been removed from
// some event but not closed, it won't call that event? Think so...
unsigned int
Poll::process() {
  unsigned int count = 0;

  for (epoll_event *itr = m_internal->m_events.get(), *last = m_internal->m_events.get() + m_internal->m_waiting_events; itr != last; ++itr) {
    if (itr->data.fd < 0)
      throw internal_error("Poll::process() received negative file descriptor: " + std::to_string(itr->data.fd));

    if (static_cast<size_t>(itr->data.fd) >= m_internal->m_table.size())
      throw internal_error("Poll::process() received invalid file descriptor: " + std::to_string(itr->data.fd));

    if (utils::Thread::self()->callbacks_should_interrupt_polling())
      utils::Thread::self()->process_callbacks(true);

    auto evItr = m_internal->m_table.begin() + itr->data.fd;

    if (evItr->second == nullptr) {
      LT_LOG_DEBUG_DATA_FD("event is null, skipping : events:%hx", itr->events);
      continue;
    }

    // Each branch must check for data.ptr != nullptr to allow the socket
    // to remove itself between the calls.
    //
    // TODO: Make it so that it checks that read/write is wanted, that
    // it wasn't removed from one of them but not closed.

    if (itr->events & EPOLLERR && evItr->first & EPOLLERR) {
      count++;
      evItr->second->event_error();

      // We assume that the event gets closed if we get an error.
      continue;
    }

    if (itr->events & EPOLLIN && evItr->first & EPOLLIN) {
      count++;
      evItr->second->event_read();
    }

    if (itr->events & EPOLLOUT && evItr->first & EPOLLOUT) {
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

  // Clear the event list just in case we open a new socket with the
  // same fd while in the middle of calling Poll::perform.
  //
  // Removed.
  //
  // Shouldn't be needed as we unset the read/write/error flags in m_internal->m_events using
  // remove_read/write/error.
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

  LT_LOG_EVENT("insert read", 0);

  m_internal->modify(event,
                     m_internal->event_mask(event) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
                     m_internal->event_mask(event) | EPOLLIN);
}

void
Poll::insert_write(Event* event) {
  if (m_internal->event_mask(event) & EPOLLOUT)
    return;

  LT_LOG_EVENT("insert write", 0);

  m_internal->modify(event,
                     m_internal->event_mask(event) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
                     m_internal->event_mask(event) | EPOLLOUT);
}

void
Poll::insert_error(Event* event) {
  if (m_internal->event_mask(event) & EPOLLERR)
    return;

  LT_LOG_EVENT("insert error", 0);

  m_internal->modify(event,
                     m_internal->event_mask(event) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
                     m_internal->event_mask(event) | EPOLLERR);
}

void
Poll::remove_read(Event* event) {
  if (!(m_internal->event_mask(event) & EPOLLIN))
    return;

  LT_LOG_EVENT("remove read", 0);

  uint32_t mask = m_internal->event_mask(event) & ~EPOLLIN;
  m_internal->modify(event,
                     mask ? EPOLL_CTL_MOD : EPOLL_CTL_DEL,
                     mask);
}

void
Poll::remove_write(Event* event) {
  if (!(m_internal->event_mask(event) & EPOLLOUT))
    return;

  LT_LOG_EVENT("remove write", 0);

  uint32_t mask = m_internal->event_mask(event) & ~EPOLLOUT;
  m_internal->modify(event,
                     mask ? EPOLL_CTL_MOD : EPOLL_CTL_DEL,
                     mask);
}

void
Poll::remove_error(Event* event) {
  if (!(m_internal->event_mask(event) & EPOLLERR))
    return;

  LT_LOG_EVENT("remove error", 0);

  uint32_t mask = m_internal->event_mask(event) & ~EPOLLERR;
  m_internal->modify(event,
                     mask ? EPOLL_CTL_MOD : EPOLL_CTL_DEL,
                     mask);
}

void
Poll::remove_and_close(Event* event) {
  remove_read(event);
  remove_write(event);
  remove_error(event);

  close(event);
}

} // namespace torrent

#endif // USE_EPOLL
