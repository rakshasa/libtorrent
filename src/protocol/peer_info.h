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

#ifndef LIBTORRENT_PEER_PEER_INFO_H
#define LIBTORRENT_PEER_PEER_INFO_H

#include <string>
#include <cstring>
#include <inttypes.h>

#include <rak/socket_address.h>

namespace torrent {

class PeerInfo {
public:
  PeerInfo();

  bool                is_valid() const                      { return m_id.length() == 20 && m_address.is_valid(); }

  bool                is_connected() const                  { return m_connected; }
  void                set_connected(bool v)                 { m_connected = v; }

  bool                is_incoming() const                   { return m_incoming; }
  void                set_incoming(bool v)                  { m_incoming = v; }

  const std::string&  get_id() const                        { return m_id; }
  void                set_id(const std::string& id)         { m_id = id; }

  char*               get_options()                         { return m_options; }
  const char*         get_options() const                   { return m_options; }

  rak::socket_address*       socket_address()               { return &m_address; }
  const rak::socket_address* socket_address() const         { return &m_address; }

  void                set_socket_address(const rak::socket_address* sa) { m_address = *sa; }

  uint32_t            failed_counter() const                { return m_failedCounter; }
  void                inc_failed_counter()                  { m_failedCounter++; }
  void                set_failed_counter(uint32_t c)        { m_failedCounter = c; }

private:
  std::string         m_id;
  rak::socket_address m_address;
  char                m_options[8];

  bool                m_connected;
  bool                m_incoming;

  uint32_t            m_failedCounter;
};

inline
PeerInfo::PeerInfo() : 
  m_connected(false),
  m_incoming(false),
  m_failedCounter(0)
{
  m_address.clear();
  std::memset(m_options, 0, 8);
}

} // namespace torrent

#endif
