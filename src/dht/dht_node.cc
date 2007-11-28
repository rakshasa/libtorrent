// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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
#include "globals.h"

#include "torrent/exceptions.h"
#include "torrent/object.h"

#include "dht_node.h"

namespace torrent {

DhtNode::DhtNode(const HashString& id, const rak::socket_address* sa) :
  HashString(id),
  m_socketAddress(*sa),
  m_lastSeen(0),
  m_recentlyActive(false),
  m_recentlyInactive(0),
  m_bucket(NULL) {

  if (sa->family() != rak::socket_address::af_inet)
    throw resource_error("Address not af_inet");
}

DhtNode::DhtNode(const std::string& id, const Object& cache) :
  HashString(*HashString::cast_from(id.c_str())),
  m_recentlyActive(false),
  m_recentlyInactive(0),
  m_bucket(NULL) {

  rak::socket_address_inet* sa = m_socketAddress.sa_inet();
  sa->set_family();
  sa->set_address_h(cache.get_key_value("i"));
  sa->set_port(cache.get_key_value("p"));
  m_lastSeen = cache.get_key_value("t");
  update();
}

char*
DhtNode::store_compact(char* buffer) const {
  HashString::cast_from(buffer)->assign(data());
  *(uint32_t*) (buffer+20) = address()->sa_inet()->address_n();
  *(uint16_t*) (buffer+24) = address()->sa_inet()->port_n();
  return buffer+26;
}

Object*
DhtNode::store_cache(Object* container) const {
  container->insert_key("i", m_socketAddress.sa_inet()->address_h());
  container->insert_key("p", m_socketAddress.sa_inet()->port());
  container->insert_key("t", m_lastSeen);
  return container;
}

}
