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
#include "globals.h"

#include "torrent/exceptions.h"
#include "torrent/object.h"
#include "torrent/utils/log.h"

#include "net/address_list.h" // For SA.

#include "dht_node.h"

#define LT_LOG_THIS(log_fmt, ...)                                       \
  lt_log_print_hash(torrent::LOG_DHT_NODE, this->id(), "dht_node", log_fmt, __VA_ARGS__);

namespace torrent {

DhtNode::DhtNode(const HashString& id, const rak::socket_address* sa) :
  HashString(id),
  m_socketAddress(*sa),
  m_lastSeen(0),
  m_recentlyActive(false),
  m_recentlyInactive(0),
  m_bucket(NULL) {

  // TODO: Change this to use the id hash similar to how peer info
  // hash'es are logged.
  LT_LOG_THIS("created (address:%s)", sa->pretty_address_str().c_str());

  // if (sa->family() != rak::socket_address::af_inet &&
  //     (sa->family() != rak::socket_address::af_inet6 || !sa->sa_inet6()->is_any()))
  //   throw resource_error("Address not af_inet or in6addr_any");
}

DhtNode::DhtNode(const std::string& id, const Object& cache) :
  HashString(*HashString::cast_from(id.c_str())),
  m_recentlyActive(false),
  m_recentlyInactive(0),
  m_bucket(NULL) {

  // TODO: Check how DHT handles inet6.
  rak::socket_address_inet* sa = m_socketAddress.sa_inet();

  sa->set_family();
  sa->set_address_h(cache.get_key_value("i"));
  sa->set_port(cache.get_key_value("p"));

  m_lastSeen = cache.get_key_value("t");

  LT_LOG_THIS("initializing (address:%s)", sa->address_str().c_str());

  update();
}

char*
DhtNode::store_compact(char* buffer) const {
  HashString::cast_from(buffer)->assign(data());

  SocketAddressCompact sa(address()->sa_inet());
  std::memcpy(buffer + 20, sa.c_str(), 6);

  return buffer + 26;
}

Object*
DhtNode::store_cache(Object* container) const {
  if (m_socketAddress.family() == rak::socket_address::af_inet6) {
    // Currently, all we support is in6addr_any (checked in the constructor),
    // which is effectively equivalent to this. Note that we need to specify
    // int64_t explicitly here because a zero constant is special in C++ and
    // thus we need an explicit match.
    container->insert_key("i", int64_t(0));
    container->insert_key("p", m_socketAddress.sa_inet6()->port());

  } else {
    container->insert_key("i", m_socketAddress.sa_inet()->address_h());
    container->insert_key("p", m_socketAddress.sa_inet()->port());
  }

  container->insert_key("t", m_lastSeen);
  return container;
}

}
