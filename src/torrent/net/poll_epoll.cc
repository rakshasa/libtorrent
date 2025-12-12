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

#define LT_LOG(log_fmt, ...)                                        \
  lt_log_print(LOG_CONNECTION_FD, "epoll: " log_fmt, __VA_ARGS__);

#define LT_LOG_EVENT(log_fmt, ...)                                      \
  lt_log_print(LOG_CONNECTION_FD, "epoll->%i : %s : " log_fmt, event->file_descriptor(), event->type_name(), __VA_ARGS__);

#define LT_LOG_DEBUG_DATA_FD(log_fmt, ...)
// #define LT_LOG_DEBUG_DATA_FD(log_fmt, ...)                                \
//   lt_log_print(LOG_CONNECTION_FD, "epoll->%i: " log_fmt, itr->data.fd, __VA_ARGS__);

namespace torrent::net {

class PollEvent {
public:
  PollEvent(Event* event) : event(event) {}
  ~PollEvent() = default;

  uint32_t            mask{};
  Event*              event{};
};

class PollInternal {
public:
  using Table = std::map<unsigned int, std::shared_ptr<PollEvent>>;

  uint32_t            event_mask(Event* event);
  void                set_event_mask(Event* event, uint32_t mask);

  void                flush();
  void                modify(torrent::Event* event, unsigned short op, uint32_t mask);

  int                 m_fd;

  unsigned int        m_max_sockets{};
  unsigned int        m_max_events{};
  unsigned int        m_waiting_events{};

  Table                                 m_table;
  std::unique_ptr<struct epoll_event[]> m_events;
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
PollInternal::modify(Event* event, unsigned short op, uint32_t mask) {
  if (event_mask(event) == mask)
    return;

  LT_LOG_EVENT("modify event : op:%hx mask:%hx", op, mask);

  if (event->m_poll_event == nullptr)
    throw internal_error("PollInternal::modify() event has no PollEvent associated: " + event->print_name_fd_str());

  epoll_event e{};
  e.data.ptr = event->m_poll_event.get();
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
               "PollInternal::modify() epoll_ctl(%d, %d -> %d, %d, [%p:%x]) = %d: %s",
               m_fd, op, retry, event->file_descriptor(), event, mask, errno, strerror(errno));

      throw internal_error(errmsg);
    }
  }
}

std::unique_ptr<Poll>
Poll::create() {
  auto socket_open_max = sysconf(_SC_OPEN_MAX);

  if (socket_open_max == -1)
    throw internal_error("Poll::create() : sysconf(_SC_OPEN_MAX) failed : " + std::string(std::strerror(errno)));

  int fd = epoll_create(socket_open_max);

  if (fd == -1)
    throw internal_error("Poll::create() : kqueue() failed : " + std::string(std::strerror(errno)));

  auto poll = new Poll();

  poll->m_internal                = std::make_unique<PollInternal>();
  poll->m_internal->m_fd          = fd;
  poll->m_internal->m_max_sockets = static_cast<unsigned int>(socket_open_max);
  poll->m_internal->m_max_events  = 1024;
  poll->m_internal->m_events      = std::make_unique<struct epoll_event[]>(poll->m_internal->m_max_events);

  return std::unique_ptr<Poll>(poll);
}

Poll::~Poll() {
  assert(m_internal->m_table.empty() && "Poll::~Poll() called with non-empty event table.");

  ::close(m_internal->m_fd);
  m_internal->m_fd = -1;
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
    if (utils::Thread::self()->callbacks_should_interrupt_polling())
      utils::Thread::self()->process_callbacks(true);

    auto* poll_event = static_cast<PollEvent*>(itr->data.ptr);
    auto* event      = poll_event->event;

    if (event == nullptr) {
      // TODO: This should fail.
      LT_LOG_DEBUG_DATA_FD("event is null, skipping : events:%hx", itr->events);
      continue;
    }

    // Each branch must check for data.ptr != nullptr to allow the socket
    // to remove itself between the calls.
    //
    // TODO: Make it so that it checks that read/write is wanted, that
    // it wasn't removed from one of them but not closed.

    if (itr->events & EPOLLERR && poll_event->mask & EPOLLERR) {
      if (!(poll_event->mask & EPOLLERR))
        throw internal_error("Poll::process() received error event for event not in error: " + poll_event->event->print_name_fd_str());

      auto event_info = event->print_name_fd_str();

      event->event_error();

      if (poll_event->mask != 0)
        throw internal_error("Poll::process() event_error called but event mask not cleared: " + event_info);

      // We assume that the event gets closed if we get an error.
      count++;
      continue;
    }

    if (itr->events & EPOLLIN && poll_event->mask & EPOLLIN) {
      event->event_read();
      count++;
    }

    if (itr->events & EPOLLOUT && poll_event->mask & EPOLLOUT) {
      event->event_write();
      count++;
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
    throw internal_error("Poll::open() invalid file descriptor for event: " + event->print_name_fd_str());

  if (event->m_poll_event != nullptr)
    throw internal_error("Poll::open() called but the event is already associated with a poll: " + event->print_name_fd_str());

  if (m_internal->m_table.find(event->file_descriptor()) != m_internal->m_table.end())
    throw internal_error("Poll::open() event already exists: " + event->print_name_fd_str());

  event->m_poll_event = std::make_shared<PollEvent>(event);

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

  poll_event->event = nullptr;
  event->m_poll_event.reset();
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
