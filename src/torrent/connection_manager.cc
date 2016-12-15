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

#include <sys/types.h>

#include <rak/address_info.h>
#include <rak/socket_address.h>
#include <rak/timer.h>

#include <stdlib.h>
#include <map>
#include <string>

#include "net/listen.h"

#include "connection_manager.h"
#include "error.h"
#include "exceptions.h"
#include "manager.h"

namespace torrent {

namespace {

// This alleviates issue https://github.com/rakshasa/rtorrent/issues/180
// Instead of waiting for the host to resolve over and over again, we remember the
// failed host and skip it for 15 minutes.

typedef std::map<std::string, uint32_t> HostNamesMap;
typedef HostNamesMap::iterator HostNamesMapIter;

HostNamesMap UnresolvedHosts;

bool IsHostFailed(const std::string& host) {
    HostNamesMapIter i = UnresolvedHosts.find(host);
    if (i == UnresolvedHosts.end()) {
        return false;
    }
    if (rak::timer::current_seconds() < i->second) {
        return true;
    }

    // cache expired - remove the host
    UnresolvedHosts.erase(i);
    return false;
}

void RememberHostFailed(const std::string& host) {
    // add 0..14 random seconds to avoid long stalls when all hosts get
    // queried again
    UnresolvedHosts[host] = rak::timer::current_seconds() + 60 * 15 + (::random() % 15);
}

} // anonymous namespace end

// Fix TrackerUdp, etc, if this is made async.
static ConnectionManager::slot_resolver_result_type*
resolve_host(const char* host, int family, int socktype, ConnectionManager::slot_resolver_result_type slot) {
  // check cache first
  if (IsHostFailed(host)) {
    slot(NULL, -1); // maybe use some distinct error code.
    return NULL;
  }

  if (manager->main_thread_main()->is_current())
    thread_base::release_global_lock();

  rak::address_info* ai;
  int err;

  if ((err = rak::address_info::get_address_info(host, family, socktype, &ai)) != 0) {
    if (manager->main_thread_main()->is_current())
      thread_base::acquire_global_lock();

    RememberHostFailed( host );
    slot(NULL, err);
    return NULL;
  }

  rak::socket_address sa;
  sa.copy(*ai->address(), ai->length());
  rak::address_info::free_address_info(ai);
  
  if (manager->main_thread_main()->is_current())
    thread_base::acquire_global_lock();
  
  slot(sa.c_sockaddr(), 0);
  return NULL;
}

ConnectionManager::ConnectionManager() :
  m_size(0),
  m_maxSize(0),

  m_priority(iptos_throughput),
  m_sendBufferSize(0),
  m_receiveBufferSize(0),
  m_encryptionOptions(encryption_none),

  m_listen(new Listen),
  m_listen_port(0),
  m_listen_backlog(SOMAXCONN) {

  m_bindAddress = (new rak::socket_address())->c_sockaddr();
  m_localAddress = (new rak::socket_address())->c_sockaddr();
  m_proxyAddress = (new rak::socket_address())->c_sockaddr();

  rak::socket_address::cast_from(m_bindAddress)->clear();
  rak::socket_address::cast_from(m_localAddress)->clear();
  rak::socket_address::cast_from(m_proxyAddress)->clear();

  m_slot_resolver = std::bind(&resolve_host,
                              std::placeholders::_1,
                              std::placeholders::_2,
                              std::placeholders::_3,
                              std::placeholders::_4);
}

ConnectionManager::~ConnectionManager() {
  delete m_listen;

  delete m_bindAddress;
  delete m_localAddress;
  delete m_proxyAddress;
}

bool
ConnectionManager::can_connect() const {
  return m_size < m_maxSize;
}

void
ConnectionManager::set_send_buffer_size(uint32_t s) {
  m_sendBufferSize = s;
}

void
ConnectionManager::set_receive_buffer_size(uint32_t s) {
  m_receiveBufferSize = s;
}

void
ConnectionManager::set_encryption_options(uint32_t options) {
#ifdef USE_OPENSSL
  m_encryptionOptions = options;
#else
  throw input_error("Compiled without encryption support.");
#endif
}

void
ConnectionManager::set_bind_address(const sockaddr* sa) {
  const rak::socket_address* rsa = rak::socket_address::cast_from(sa);

  if (rsa->family() != rak::socket_address::af_inet)
    throw input_error("Tried to set a bind address that is not an af_inet address.");

  rak::socket_address::cast_from(m_bindAddress)->copy(*rsa, rsa->length());
}

void
ConnectionManager::set_local_address(const sockaddr* sa) {
  const rak::socket_address* rsa = rak::socket_address::cast_from(sa);

  if (rsa->family() != rak::socket_address::af_inet)
    throw input_error("Tried to set a local address that is not an af_inet address.");

  rak::socket_address::cast_from(m_localAddress)->copy(*rsa, rsa->length());
}

void
ConnectionManager::set_proxy_address(const sockaddr* sa) {
  const rak::socket_address* rsa = rak::socket_address::cast_from(sa);

  if (rsa->family() != rak::socket_address::af_inet)
    throw input_error("Tried to set a proxy address that is not an af_inet address.");

  rak::socket_address::cast_from(m_proxyAddress)->copy(*rsa, rsa->length());
}

uint32_t
ConnectionManager::filter(const sockaddr* sa) {
  if (!m_slot_filter)
    return 1;
  else
    return m_slot_filter(sa);
}

bool
ConnectionManager::listen_open(port_type begin, port_type end) {
  if (!m_listen->open(begin, end, m_listen_backlog, rak::socket_address::cast_from(m_bindAddress)))
    return false;

  m_listen_port = m_listen->port();

  return true;
}

void
ConnectionManager::listen_close() {
  m_listen->close();
}

void
ConnectionManager::set_listen_backlog(int v) {
  if (v < 1 || v >= (1 << 16))
    throw input_error("backlog value out of bounds");

  if (m_listen->is_open())
    throw input_error("backlog value must be set before listen port is opened");

  m_listen_backlog = v;
}

}
