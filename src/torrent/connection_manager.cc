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

#include "net/listen.h"

#include "connection_manager.h"
#include "error.h"
#include "exceptions.h"
#include "manager.h"

namespace torrent {

static void
resolve_host(const char* host, int family, int socktype, resolver_callback slot) {
  if (manager->main_thread_main()->is_current())
    thread_base::release_global_lock();

  rak::address_info* ai;
  int err;

  if ((err = rak::address_info::get_address_info(host, family, socktype, &ai)) != 0) {
    if (manager->main_thread_main()->is_current())
      thread_base::acquire_global_lock();

    slot(NULL, err);
    return;
  }

  rak::socket_address sa;
  sa.copy(*ai->address(), ai->length());
  rak::address_info::free_address_info(ai);
  
  if (manager->main_thread_main()->is_current())
    thread_base::acquire_global_lock();
  
  slot(sa.c_sockaddr(), 0);
  return;
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

void *
ConnectionManager::enqueue_async_resolve(const char *name, int family, resolver_callback *cbck) {
#ifdef HAVE_UDNS
    return m_udnsevent.enqueue_resolve(name, family, cbck);
#else
    MockResolve *mock_resolve = new MockResolve;
    mock_resolve->hostname = std::string(name);
    mock_resolve->family = family;
    mock_resolve->callback = cbck;
    m_mock_resolve_queue.push_back(mock_resolve);
    return mock_resolve;
#endif
}

void ConnectionManager::flush_async_resolves() {
#ifdef HAVE_UDNS
    m_udnsevent.flush_resolves();
#else
    // dequeue all callbacks and resolve them synchronously
    while (!m_mock_resolve_queue.empty()) {
        MockResolve *mock_resolve = m_mock_resolve_queue.front();
        m_mock_resolve_queue.pop_front();
        this->resolver()(mock_resolve->hostname.c_str(), mock_resolve->family, 0, *(mock_resolve->callback));
        delete mock_resolve;
    }
#endif
}

void
ConnectionManager::cancel_async_resolve(void *query) {
#ifdef HAVE_UDNS
    m_udnsevent.cancel((UdnsQuery *) query);
#else
    // O(n), meh; this should never get called in a non-udns build
    MockResolve *mock_resolve = (MockResolve *) query;
    auto it = std::find(std::begin(m_mock_resolve_queue), std::end(m_mock_resolve_queue), mock_resolve);
    if (it != std::end(m_mock_resolve_queue)) {
        m_mock_resolve_queue.erase(it);
        delete mock_resolve;
    }
#endif
}

}
