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
  PeerInfo(const std::string& id, const rak::socket_address& sa, bool incoming);

  bool                is_incoming() const                   { return m_incoming; }
  bool                is_valid() const                      { return m_id.length() == 20 && m_sa.is_valid(); }

  const std::string&  get_id() const                        { return m_id; }
  void                set_id(const std::string& id)         { m_id = id; }

  std::string         get_address() const                   { return m_sa.address_str(); }
  uint16_t            get_port() const                      { return m_sa.port(); }
  char*               get_options()                         { return m_options; }
  const char*         get_options() const                   { return m_options; }

  rak::socket_address&       get_socket_address()                 { return m_sa; }
  const rak::socket_address& get_socket_address() const           { return m_sa; }

  bool                operator < (const PeerInfo& p) const  { return m_id < p.m_id; }
  bool                operator == (const PeerInfo& p) const { return m_id == p.m_id; }

private:
  std::string         m_id;
  rak::socket_address m_sa;
  char                m_options[8];

  bool                m_incoming;
};

inline
PeerInfo::PeerInfo() :
  m_incoming(false) {
  m_sa.clear();
  std::memset(m_options, 0, 8);
}

inline
PeerInfo::PeerInfo(const std::string& id, const rak::socket_address& sa, bool incoming) :
  m_id(id),
  m_sa(sa),
  m_incoming(incoming) {
  std::memset(m_options, 0, 8);
}

} // namespace torrent

#endif // LIBTORRENT_PEER_INFO_H
