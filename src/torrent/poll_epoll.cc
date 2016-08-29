// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include <cerrno>
#include <cstring>
#include <cstdio>

#include <stdexcept>
#include <unistd.h>
#include <torrent/exceptions.h>
#include <torrent/event.h>

#include "torrent.h"
#include "poll_epoll.h"
#include "utils/log.h"
#include "utils/thread_base.h"
#include "rak/error_number.h"
#include "rak/timer.h"

#ifdef USE_EPOLL
#include <sys/epoll.h>
#endif

#define LT_LOG_EVENT(event, log_level, log_fmt, ...)                    \
  lt_log_print(LOG_SOCKET_##log_level, "epoll->%s(%i): " log_fmt, event->type_name(), event->file_descriptor(), __VA_ARGS__);

namespace torrent {

#ifdef USE_EPOLL

inline uint32_t
PollEPoll::event_mask(Event* e) {
  Table::value_type entry = m_table[e->file_descriptor()];
  return entry.second != e ? 0 : entry.first;
}

inline void
PollEPoll::set_event_mask(Event* e, uint32_t m) {
  m_table[e->file_descriptor()] = Table::value_type(m, e);
}

inline void
PollEPoll::modify(Event* event, int op, uint32_t mask) {
  if (event_mask(event) == mask)
    return;

  LT_LOG_EVENT(event, DEBUG, "Modify event: op:%hx mask:%hx.", op, mask);

  epoll_event e;
  e.data.u64 = 0; // Make valgrind happy? Remove please.
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
               "PollEPoll::modify(...) epoll_ctl(%d, %d -> %d, %d, [%p:%x]) = %d: %s",
               m_fd, op, retry, event->file_descriptor(), event, mask, errno, strerror(errno));

      throw internal_error(errmsg);
    }
  }
}

PollEPoll*
PollEPoll::create(int maxOpenSockets) {
  int fd = epoll_create(maxOpenSockets);

  if (fd == -1)
    return NULL;

  return new PollEPoll(fd, 1024, maxOpenSockets);
}

PollEPoll::PollEPoll(int fd, int maxEvents, int maxOpenSockets) :
  m_fd(fd),
  m_maxEvents(maxEvents),
  m_waitingEvents(0),
  m_events(new epoll_event[m_maxEvents]) {

  m_table.resize(maxOpenSockets);
}

PollEPoll::~PollEPoll() {
  m_table.clear();
  delete [] m_events;

  ::close(m_fd);
}

int
PollEPoll::poll(int msec) {
  int nfds = epoll_wait(m_fd, m_events, m_maxEvents, msec);

  if (nfds == -1)
    return -1;

  return m_waitingEvents = nfds;
}

// We check m_table to make sure the Event is still listening to the
// event, so it is safe to remove Event's while in working.
//
// TODO: Do we want to guarantee if the Event has been removed from
// some event but not closed, it won't call that event? Think so...
unsigned int
PollEPoll::perform() {
  unsigned int count = 0;

  for (epoll_event *itr = m_events, *last = m_events + m_waitingEvents; itr != last; ++itr) {
    if (itr->data.fd < 0 || (size_t)itr->data.fd >= m_table.size())
      continue;

    if ((flags() & flag_waive_global_lock) && thread_base::global_queue_size() != 0)
      thread_base::waive_global_lock();

    Table::iterator evItr = m_table.begin() + itr->data.fd;

    // Each branch must check for data.ptr != NULL to allow the socket
    // to remove itself between the calls.
    //
    // TODO: Make it so that it checks that read/write is wanted, that
    // it wasn't removed from one of them but not closed.

    if (itr->events & EPOLLERR && evItr->second != NULL && evItr->first & EPOLLERR) {
      count++;
      evItr->second->event_error();
    }

    if (itr->events & EPOLLIN && evItr->second != NULL && evItr->first & EPOLLIN) {
      count++;
      evItr->second->event_read();
    }

    if (itr->events & EPOLLOUT && evItr->second != NULL && evItr->first & EPOLLOUT) {
      count++;
      evItr->second->event_write();
    }
  }

  m_waitingEvents = 0;
  return count;
}

unsigned int
PollEPoll::do_poll(int64_t timeout_usec, int flags) {
  rak::timer timeout = rak::timer(timeout_usec);

  timeout += 10;

  if (!(flags & poll_worker_thread)) {
    thread_base::release_global_lock();
    thread_base::entering_main_polling();
  }

  int status = poll((timeout.usec() + 999) / 1000);

  if (!(flags & poll_worker_thread)) {
    thread_base::leaving_main_polling();
    thread_base::acquire_global_lock();
  }

  if (status == -1) {
    if (rak::error_number::current().value() != rak::error_number::e_intr) {
      throw std::runtime_error("PollEPoll::work(): " + std::string(rak::error_number::current().c_str()));
    }

    return 0;
  }

  return perform();
}

uint32_t
PollEPoll::open_max() const {
  return m_table.size();
}

void
PollEPoll::open(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "Open event.", 0);

  if (event_mask(event) != 0)
    throw internal_error("PollEPoll::open(...) called but the file descriptor is active");
}

void
PollEPoll::close(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "Close event.", 0);

  if (event_mask(event) != 0)
    throw internal_error("PollEPoll::close(...) called but the file descriptor is active");

  m_table[event->file_descriptor()] = Table::value_type();

  // Clear the event list just in case we open a new socket with the
  // same fd while in the middle of calling PollEPoll::perform.
  for (epoll_event *itr = m_events, *last = m_events + m_waitingEvents; itr != last; ++itr)
    if (itr->data.fd == event->file_descriptor())
      itr->events = 0;
}

void
PollEPoll::closed(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "Closed event.", 0);

  // Kernel removes closed FDs automatically, so just clear the mask and remove it from pending calls.
  // Don't touch if the FD was re-used before we received the close notification.
  if (m_table[event->file_descriptor()].second == event)
    m_table[event->file_descriptor()] = Table::value_type();

  // for (epoll_event *itr = m_events, *last = m_events + m_waitingEvents; itr != last; ++itr) {
  //   if (itr->data.fd == event->file_descriptor())
  //     itr->events = 0;
  // }
}

// Use custom defines for EPOLL* to make the below code compile with
// and with epoll.
bool
PollEPoll::in_read(Event* event) {
  return event_mask(event) & EPOLLIN;
}

bool
PollEPoll::in_write(Event* event) {
  return event_mask(event) & EPOLLOUT;
}

bool
PollEPoll::in_error(Event* event) {
  return event_mask(event) & EPOLLERR;
}

void
PollEPoll::insert_read(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "Insert read.", 0);

  modify(event,
	 event_mask(event) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
	 event_mask(event) | EPOLLIN);
}

void
PollEPoll::insert_write(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "Insert write.", 0);

  modify(event,
	 event_mask(event) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
	 event_mask(event) | EPOLLOUT);
}

void
PollEPoll::insert_error(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "Insert error.", 0);

  modify(event,
	 event_mask(event) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
	 event_mask(event) | EPOLLERR);
}

void
PollEPoll::remove_read(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "Remove read.", 0);

  uint32_t mask = event_mask(event) & ~EPOLLIN;
  modify(event, mask ? EPOLL_CTL_MOD : EPOLL_CTL_DEL, mask);
}

void
PollEPoll::remove_write(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "Remove write.", 0);

  uint32_t mask = event_mask(event) & ~EPOLLOUT;
  modify(event, mask ? EPOLL_CTL_MOD : EPOLL_CTL_DEL, mask);
}

void
PollEPoll::remove_error(Event* event) {
  LT_LOG_EVENT(event, DEBUG, "Remove error.", 0);

  uint32_t mask = event_mask(event) & ~EPOLLERR;
  modify(event, mask ? EPOLL_CTL_MOD : EPOLL_CTL_DEL, mask);
}

#else // USE_EPOLL

PollEPoll* PollEPoll::create(int maxOpenSockets) { return NULL; }
PollEPoll::~PollEPoll() {}

int PollEPoll::poll(int msec) { throw internal_error("An PollEPoll function was called, but it is disabled."); }
unsigned int PollEPoll::perform() { throw internal_error("An PollEPoll function was called, but it is disabled."); }
unsigned int PollEPoll::do_poll(int64_t timeout_usec, int flags) { throw internal_error("An PollEPoll function was called, but it is disabled."); }
uint32_t PollEPoll::open_max() const { throw internal_error("An PollEPoll function was called, but it is disabled."); }

void PollEPoll::open(torrent::Event* event) {}
void PollEPoll::close(torrent::Event* event) {}
void PollEPoll::closed(torrent::Event* event) {}

bool PollEPoll::in_read(torrent::Event* event) { throw internal_error("An PollEPoll function was called, but it is disabled."); }
bool PollEPoll::in_write(torrent::Event* event) { throw internal_error("An PollEPoll function was called, but it is disabled."); }
bool PollEPoll::in_error(torrent::Event* event) { throw internal_error("An PollEPoll function was called, but it is disabled."); }

void PollEPoll::insert_read(torrent::Event* event) {}
void PollEPoll::insert_write(torrent::Event* event) {}
void PollEPoll::insert_error(torrent::Event* event) {}

void PollEPoll::remove_read(torrent::Event* event) {}
void PollEPoll::remove_write(torrent::Event* event) {}
void PollEPoll::remove_error(torrent::Event* event) {}

PollEPoll::PollEPoll(int fd, int maxEvents, int maxOpenSockets) { throw internal_error("An PollEPoll function was called, but it is disabled."); }

#endif // USE_EPOLL

}
