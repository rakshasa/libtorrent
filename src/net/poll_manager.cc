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

#include "manager.h"
#include "poll_select.h"

namespace torrent {

void
PollManager::set_open_max(int s) {
  m_readSet.reserve(s);
  m_writeSet.reserve(s);
  m_exceptSet.reserve(s);
}

int
PollManager::mark(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet) {
  int maxFd = 0;

  m_readSet.prepare();
  std::for_each(m_readSet.begin(), m_readSet.end(), poll_mark(readSet, &maxFd));

  m_writeSet.prepare();
  std::for_each(m_writeSet.begin(), m_writeSet.end(), poll_mark(writeSet, &maxFd));
  
  m_exceptSet.prepare();
  std::for_each(m_exceptSet.begin(), m_exceptSet.end(), poll_mark(exceptSet, &maxFd));

  return maxFd;
}

int
PollManager::work(fd_set* readSet, fd_set* writeSet, fd_set* exceptSet, int maxFd) {
  // Make sure we don't do read/write on fd's that are in except. This should
  // not be a problem as any except call should remove it from the m_*Set's.
  m_exceptSet.prepare();
  std::for_each(m_exceptSet.begin(), m_exceptSet.end(),
		poll_check(exceptSet, std::mem_fun(&SocketBase::except)));

  m_readSet.prepare();
  std::for_each(m_readSet.begin(), m_readSet.end(),
		poll_check(readSet, std::mem_fun(&SocketBase::read)));

  m_writeSet.prepare();
  std::for_each(m_writeSet.begin(), m_writeSet.end(),
		poll_check(writeSet, std::mem_fun(&SocketBase::write)));
}

}
