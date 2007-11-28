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

#include <torrent/exceptions.h>

#include "manager.h"
#include "dht/dht_router.h"
#include "net/throttle_manager.h"

#include "dht_manager.h"

namespace torrent {

DhtManager::~DhtManager() {
  stop();
  delete m_router;
}

void
DhtManager::initialize(const Object& dhtCache) {
  if (m_router != NULL)
    throw internal_error("DhtManager::initialize called with DHT already active.");

  m_router = new DhtRouter(dhtCache, rak::socket_address::cast_from(manager->connection_manager()->bind_address()));
}

void
DhtManager::start(port_type port) {
  if (m_router == NULL)
    throw internal_error("DhtManager::start called without initializing first.");

  m_port = port;
  m_router->start(port);
}


void
DhtManager::stop() {
  if (m_router != NULL)
    m_router->stop();
}

bool
DhtManager::is_active() const {
  return m_router != NULL && m_router->is_active();
}

void
DhtManager::add_node(const sockaddr* addr, int port) {
  if (m_router != NULL)
    m_router->contact(rak::socket_address::cast_from(addr), port);
}

void
DhtManager::add_node(const std::string& host, int port) {
  if (m_router != NULL)
    m_router->add_contact(host, port);
}

Object*
DhtManager::store_cache(Object* container) const {
  if (m_router == NULL)
    throw internal_error("DhtManager::store_cache called but DHT not initialized.");

  return m_router->store_cache(container);
}

DhtManager::statistics_type
DhtManager::get_statistics() const {
  return m_router->get_statistics();
}

void
DhtManager::reset_statistics() {
  m_router->reset_statistics();
}

}
