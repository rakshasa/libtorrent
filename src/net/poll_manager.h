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

#ifndef LIBTORRENT_NET_POLL_MANAGER_H
#define LIBTORRENT_NET_POLL_MANAGER_H

#include <sys/types.h>
#include <sys/select.h>

#include "socket_set.h"

namespace torrent {

// Note that when you move file descriptors between SocketBase
// objects, you must remove the SocketBase object from the
// Poll::*_set()'s before adding the target SocketBase to the set's or
// clearing the source SocketBase's fd.

class PollManager {
public:
  
  void                set_open_max(int s);

  int                 mark(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet);
  void                work(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int maxFd);

  SocketSet&          read_set() { return m_readSet; }
  SocketSet&          write_set() { return m_writeSet; }
  SocketSet&          except_set() { return m_exceptSet; }

private:
  SocketSet           m_readSet;
  SocketSet           m_writeSet;
  SocketSet           m_exceptSet;
};

}

#endif
