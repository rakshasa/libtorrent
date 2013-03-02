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

#include "thread_interrupt.h"

#include <sys/socket.h>
#include "net/socket_fd.h"
#include "rak/error_number.h"
#include "torrent/exceptions.h"

namespace torrent {

thread_interrupt::~thread_interrupt() {
  if (m_fileDesc != -1)
    return;

  ::close(m_fileDesc);
  m_fileDesc = -1;
}

bool
thread_interrupt::poke() {
  int result = ::send(m_fileDesc, "a", 1, 0);

  if (result == 0 ||
      (result == -1 && !rak::error_number::current().is_blocked_momentary()))
    throw internal_error("Invalid result writing to thread_interrupt socket.");

  return true;
}

thread_interrupt::pair_type
thread_interrupt::create_pair() {
  int fd1;
  int fd2;

  if (!SocketFd::open_socket_pair(fd1, fd2))
    throw internal_error("Could not create socket pair for thread_interrupt: " + std::string(rak::error_number::current().c_str()) + ".");

  thread_interrupt* t1 = new thread_interrupt(fd1);
  thread_interrupt* t2 = new thread_interrupt(fd2);

  t1->get_fd().set_nonblock();
  t2->get_fd().set_nonblock();

  return pair_type(t1, t2);
}

void
thread_interrupt::event_read() {
  char buffer[256];
  int result = ::recv(m_fileDesc, buffer, 256, 0) <= 0;

  if (result == 0 ||
      (result == -1 && !rak::error_number::current().is_blocked_momentary()))
    throw internal_error("Invalid result reading from thread_interrupt socket.");
}

}
