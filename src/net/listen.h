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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_LISTEN_H
#define LIBTORRENT_LISTEN_H

#include <inttypes.h>
#include <sigc++/signal.h>

#include "socket_base.h"

namespace torrent {

class Listen : public SocketBase {
public:
  typedef sigc::slot3<void, int, std::string, uint16_t> SlotIncoming;

  Listen() : SocketBase(-1), m_port(0) {}
  ~Listen() { close(); }

  bool            open(uint16_t first, uint16_t last, const std::string& addr);
  void            close();

  bool            is_open()                            { return m_fd.is_valid(); }

  uint16_t        get_port()                           { return m_port; }

  // int         file descriptor
  // std::string address
  // uint16_t    port
  void            slot_incoming(const SlotIncoming& s) { m_slotIncoming = s; }

  virtual void    read();
  virtual void    write();
  virtual void    except();

private:
  Listen(const Listen&);
  void operator = (const Listen&);

  uint64_t        m_port;

  SlotIncoming    m_slotIncoming;
};

} // namespace torrent

#endif // LIBTORRENT_TORRENT_H
