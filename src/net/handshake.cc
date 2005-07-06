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

#include "torrent/exceptions.h"

#include "handshake.h"
#include "handshake_manager.h"
#include "manager.h"

namespace torrent {

Handshake::Handshake(SocketFd fd, HandshakeManager* m) :
  SocketBase(fd),
  m_manager(m),
  m_buf(new char[256 + 48]),
  m_pos(0) {

  m_taskTimeout.set_iterator(taskScheduler.end());
  m_taskTimeout.set_slot(sigc::mem_fun(*this, &Handshake::send_failed));

  taskScheduler.insert(&m_taskTimeout, Timer::cache() + 60 * 1000000);
}

Handshake::~Handshake() {
  taskScheduler.erase(&m_taskTimeout);

  if (m_fd.is_valid())
    throw internal_error("Handshake dtor called but m_fd is still open");

  delete [] m_buf;
}

void
Handshake::clear_poll() {
  pollManager.read_set().erase(this);
  pollManager.write_set().erase(this);
  pollManager.except_set().erase(this);
}

// TODO: Move the management of the socketfd to handshake_manager?
void
Handshake::close() {
  if (!m_fd.is_valid())
    return;

  clear_poll();
  
  socketManager.close(m_fd);
  m_fd.clear();
}

void
Handshake::send_connected() {
  m_manager->receive_connected(this);
}

void
Handshake::send_failed() {
  m_manager->receive_failed(this);
}

bool
Handshake::recv1() {
  if (m_pos == 0 && !read_buffer(m_buf, 1, m_pos))
    return false;

  unsigned int len = (unsigned char)m_buf[0];

  if (!read_buffer(m_buf + m_pos, len + 29, m_pos))
    return false;

  m_peer.set_options(std::string(m_buf + 1 + len, 8));
  m_hash = std::string(m_buf + 9 + len, 20);

  if (std::string(m_buf + 1, len) != "BitTorrent protocol")
    throw communication_error("Peer returned wrong protocol identifier");

  return true;
}

bool
Handshake::recv2() {
  if (!read_buffer(m_buf + m_pos, 20, m_pos))
    return false;

  m_peer.set_id(std::string(m_buf, 20));

  return true;
}  

}
