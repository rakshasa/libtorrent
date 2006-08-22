// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#include <unistd.h>
#include <torrent/exceptions.h>
#include <torrent/event.h>

#include "poll_epoll.h"

#ifdef USE_EPOLL
#include <sys/epoll.h>
#endif

namespace torrent {

#ifdef USE_EPOLL

inline uint32_t
PollEPoll::event_mask(Event* e) {
  return m_table[e->file_descriptor()];
}

inline void
PollEPoll::set_event_mask(Event* e, uint32_t m) {
  m_table[e->file_descriptor()] = m;
}

inline void
PollEPoll::modify(Event* event, int op, uint32_t mask) {
  if (event_mask(event) == mask)
    return;

  epoll_event e;
  e.data.u64 = 0; // Make valgrind happy? Remove please.
  e.data.ptr = event;
  e.events = mask;

  set_event_mask(event, mask);

  if (epoll_ctl(m_fd, op, event->file_descriptor(), &e))
    throw internal_error("PollEPoll::insert_read(...) epoll_ctl call failed");
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
void
PollEPoll::perform() {
  for (epoll_event *itr = m_events, *last = m_events + m_waitingEvents; itr != last; ++itr) {
    // Each branch must check for data.ptr != NULL to allow the socket
    // to remove itself between the calls.
    //
    // TODO: Make it so that it checks that read/write is wanted, that
    // it wasn't removed from one of them but not closed.

    if (itr->events & EPOLLERR && itr->data.ptr != NULL && event_mask((Event*)itr->data.ptr) & EPOLLERR)
      ((Event*)itr->data.ptr)->event_error();

    if (itr->events & EPOLLIN && itr->data.ptr != NULL && event_mask((Event*)itr->data.ptr) & EPOLLIN)
      ((Event*)itr->data.ptr)->event_read();

    if (itr->events & EPOLLOUT && itr->data.ptr != NULL && event_mask((Event*)itr->data.ptr) & EPOLLOUT)
      ((Event*)itr->data.ptr)->event_write();
  }

  m_waitingEvents = 0;
}

uint32_t
PollEPoll::open_max() const {
  return m_table.size();
}

void
PollEPoll::open(Event* event) {
  if (event_mask(event) != 0)
    throw internal_error("PollEPoll::open(...) called but the file descriptor is active");
}

void
PollEPoll::close(Event* event) {
  if (event_mask(event) != 0)
    throw internal_error("PollEPoll::close(...) called but the file descriptor is active");

  for (epoll_event *itr = m_events, *last = m_events + m_waitingEvents; itr != last; ++itr)
    if (itr->data.ptr == event)
      itr->data.ptr = NULL;
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
  modify(event,
	 event_mask(event) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
	 event_mask(event) | EPOLLIN);
}

void
PollEPoll::insert_write(Event* event) {
  modify(event,
	 event_mask(event) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
	 event_mask(event) | EPOLLOUT);
}

void
PollEPoll::insert_error(Event* event) {
  modify(event,
	 event_mask(event) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD,
	 event_mask(event) | EPOLLERR);
}

void
PollEPoll::remove_read(Event* event) {
  uint32_t mask = event_mask(event) & ~EPOLLIN;

  modify(event, mask ? EPOLL_CTL_MOD : EPOLL_CTL_DEL, mask);
}

void
PollEPoll::remove_write(Event* event) {
  uint32_t mask = event_mask(event) & ~EPOLLOUT;

  modify(event, mask ? EPOLL_CTL_MOD : EPOLL_CTL_DEL, mask);
}

void
PollEPoll::remove_error(Event* event) {
  uint32_t mask = event_mask(event) & ~EPOLLERR;

  modify(event, mask ? EPOLL_CTL_MOD : EPOLL_CTL_DEL, mask);
}

#else // USE_EPOLL

PollEPoll*
PollEPoll::create(int maxOpenSockets) {
  return NULL;
}

PollEPoll::~PollEPoll() {
}

int
PollEPoll::poll(int msec) {
  throw internal_error("An PollEPoll function was called, but it is disabled.");
}

void
PollEPoll::perform() {
  throw internal_error("An PollEPoll function was called, but it is disabled.");
}

uint32_t
PollEPoll::open_max() const {
  throw internal_error("An PollEPoll function was called, but it is disabled.");
}

void
PollEPoll::open(torrent::Event* event) {
}

void
PollEPoll::close(torrent::Event* event) {
}

bool
PollEPoll::in_read(torrent::Event* event) {
  throw internal_error("An PollEPoll function was called, but it is disabled.");
}

bool
PollEPoll::in_write(torrent::Event* event) {
  throw internal_error("An PollEPoll function was called, but it is disabled.");
}

bool
PollEPoll::in_error(torrent::Event* event) {
  throw internal_error("An PollEPoll function was called, but it is disabled.");
}

void
PollEPoll::insert_read(torrent::Event* event) {
}

void
PollEPoll::insert_write(torrent::Event* event) {
}

void
PollEPoll::insert_error(torrent::Event* event) {
}

void
PollEPoll::remove_read(torrent::Event* event) {
}

void
PollEPoll::remove_write(torrent::Event* event) {
}

void
PollEPoll::remove_error(torrent::Event* event) {
}

PollEPoll::PollEPoll(int fd, int maxEvents, int maxOpenSockets) {
  throw internal_error("An PollEPoll function was called, but it is disabled.");
}

#endif // USE_EPOLL

}
