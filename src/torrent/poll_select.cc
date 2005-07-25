// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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

#include <unistd.h>

#include "net/socket_set.h"

#include "event.h"
#include "exceptions.h"
#include "poll_select.h"

namespace torrent {

template <typename _Operation>
struct poll_check_t {
  poll_check_t(fd_set* s, _Operation op) : m_set(s), m_op(op) {}

  void operator () (Event* s) {
//     if (s == NULL)
//       throw internal_error("poll_mark: s == NULL");

    if (FD_ISSET(s->get_file_desc(), m_set))
      m_op(s);
  }

  fd_set*    m_set;
  _Operation m_op;
};

template <typename _Operation>
inline poll_check_t<_Operation>
poll_check(fd_set* s, _Operation op) {
  return poll_check_t<_Operation>(s, op);
}

struct poll_mark {
  poll_mark(fd_set* s, int* m) : m_max(m), m_set(s) {}

  void operator () (Event* s) {
//     if (s == NULL)
//       throw internal_error("poll_mark: s == NULL");

    *m_max = std::max(*m_max, s->get_file_desc());

    FD_SET(s->get_file_desc(), m_set);
  }

  int*    m_max;
  fd_set* m_set;
};

PollSelect::PollSelect() :
  m_readSet(new SocketSet),
  m_writeSet(new SocketSet),
  m_exceptSet(new SocketSet) {
  set_open_max(sysconf(_SC_OPEN_MAX));
}

PollSelect::~PollSelect() {
//   if (m_readSet->empty() || !m_writeSet->empty() || !m_exceptSet->empty())
//     throw internal_error("PollSelect::~PollSelect() called but the sets are not empty");

  delete m_readSet;
  delete m_writeSet;
  delete m_exceptSet;
}

void
PollSelect::set_open_max(int s) {
  if (s <= 0)
    throw internal_error("PollSelect::set_open_max(...) received an invalid value");

  m_readSet->reserve(s);
  m_writeSet->reserve(s);
  m_exceptSet->reserve(s);
}

int
PollSelect::mark(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet) {
  int maxFd = 0;

  m_readSet->prepare();
  std::for_each(m_readSet->begin(), m_readSet->end(), poll_mark(readSet, &maxFd));

  m_writeSet->prepare();
  std::for_each(m_writeSet->begin(), m_writeSet->end(), poll_mark(writeSet, &maxFd));
  
  m_exceptSet->prepare();
  std::for_each(m_exceptSet->begin(), m_exceptSet->end(), poll_mark(exceptSet, &maxFd));

  return maxFd;
}

void
PollSelect::work(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet) {
  // Make sure we don't do read/write on fd's that are in except. This should
  // not be a problem as any except call should remove it from the m_*Set's.
  m_exceptSet->prepare();
  std::for_each(m_exceptSet->begin(), m_exceptSet->end(),
		poll_check(exceptSet, std::mem_fun(&Event::event_error)));

  m_readSet->prepare();
  std::for_each(m_readSet->begin(), m_readSet->end(),
		poll_check(readSet, std::mem_fun(&Event::event_read)));

  m_writeSet->prepare();
  std::for_each(m_writeSet->begin(), m_writeSet->end(),
		poll_check(writeSet, std::mem_fun(&Event::event_write)));
}

void
PollSelect::open(Event* event) {
  if (event->get_file_desc() < 0)
    throw internal_error("PollSelect::open(...) called with an invalid file descriptor");

  if (in_read(event) || in_write(event) || in_error(event))
    throw internal_error("PollSelect::open(...) called on an inserted event");
}

void
PollSelect::close(Event* event) {
  if (event->get_file_desc() < 0)
    throw internal_error("PollSelect::close(...) called with an invalid file descriptor");

  if (in_read(event) || in_write(event) || in_error(event))
    throw internal_error("PollSelect::close(...) called on an inserted event");
}

bool
PollSelect::in_read(Event* event) {
  return m_readSet->find(event) != m_readSet->end();
}

bool
PollSelect::in_write(Event* event) {
  return m_writeSet->find(event) != m_writeSet->end();
}

bool
PollSelect::in_error(Event* event) {
  return m_exceptSet->find(event) != m_exceptSet->end();
}

void
PollSelect::insert_read(Event* event) {
  m_readSet->insert(event);
}

void
PollSelect::insert_write(Event* event) {
  m_writeSet->insert(event);
}

void
PollSelect::insert_error(Event* event) {
  m_exceptSet->insert(event);
}

void
PollSelect::remove_read(Event* event) {
  m_readSet->erase(event);
}

void
PollSelect::remove_write(Event* event) {
  m_writeSet->erase(event);
}

void
PollSelect::remove_error(Event* event) {
  m_exceptSet->erase(event);
}

}
