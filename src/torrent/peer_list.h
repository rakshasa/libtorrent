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

#ifndef LIBTORRENT_PEER_LIST_H
#define LIBTORRENT_PEER_LIST_H

#include <map>

struct sockaddr;

namespace torrent {

class AvailableList;
class ConnectionList;
class Handshake;
class HandshakeManager;
class PeerInfo;

// Unique key for the address, excluding port numbers etc.
class socket_address_key {
public:
  socket_address_key(const sockaddr* sa) : m_sockaddr(sa) {}

  bool operator < (const socket_address_key& sa) const;

private:
  const sockaddr*     m_sockaddr;
};

class PeerList : private std::multimap<socket_address_key, PeerInfo*> {
public:
  friend class Handshake;
  friend class HandshakeManager;
  friend class ConnectionList;

  typedef std::multimap<socket_address_key, PeerInfo*>        base_type;
  typedef std::pair<base_type::iterator, base_type::iterator> range_type;

  using base_type::value_type;
  using base_type::reference;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::const_iterator;
  using base_type::const_reverse_iterator;

  using base_type::size;
  using base_type::empty;

  static const int address_available       = (1 << 0);

  static const int connect_incoming        = (1 << 0);
  static const int connect_keep_handshakes = (1 << 1);

  // Make sure any change here match ConnectionList's flags.
  static const int disconnect_available    = (1 << 0);
  static const int disconnect_quick        = (1 << 1);
  static const int disconnect_unwanted     = (1 << 2);

  static const int cull_old                = (1 << 0);
  static const int cull_keep_interesting   = (1 << 1);

  PeerList();
  ~PeerList();

  PeerInfo*           insert_address(const sockaddr* address, int flags);

  AvailableList*      available_list()  { return m_availableList; }

  uint32_t            cull_peers(int flags);

  const_iterator         begin() const  { return base_type::begin(); }
  const_iterator         end() const    { return base_type::end(); }
  const_reverse_iterator rbegin() const { return base_type::rbegin(); }
  const_reverse_iterator rend() const   { return base_type::rend(); }

protected:
  // Insert, or find a PeerInfo with socket address 'sa'. Returns end
  // if no more connections are allowed from that host.
  PeerInfo*           connected(const sockaddr* sa, int flags);

  void                disconnected(PeerInfo* p, int flags);
  iterator            disconnected(iterator itr, int flags);

private:
  PeerList(const PeerList&);
  void operator = (const PeerList&);

  AvailableList*      m_availableList;
};

}

#endif
