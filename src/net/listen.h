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

#ifndef LIBTORRENT_LISTEN_H
#define LIBTORRENT_LISTEN_H

#include <cinttypes>
#include <functional>
#include <rak/socket_address.h>

#include "socket_base.h"
#include "socket_fd.h"

namespace torrent {

class Listen : public SocketBase {
public:
  typedef std::function<void (SocketFd, const rak::socket_address&)> slot_connection;

  Listen() : m_port(0) {}
  ~Listen() { close(); }

  bool                open(uint16_t first, uint16_t last, int backlog, const rak::socket_address* bindAddress);
  void                close();

  bool                is_open() const { return get_fd().is_valid(); }

  uint16_t            port() const { return m_port; }

  slot_connection&    slot_accepted() { return m_slot_accepted; }

  virtual void        event_read();
  virtual void        event_write();
  virtual void        event_error();

private:
  uint64_t            m_port;

  slot_connection     m_slot_accepted;
};

} // namespace torrent

#endif // LIBTORRENT_TORRENT_H
