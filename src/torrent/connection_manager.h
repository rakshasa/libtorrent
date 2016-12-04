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

// Add some helpfull words here.

#ifndef LIBTORRENT_CONNECTION_MANAGER_H
#define LIBTORRENT_CONNECTION_MANAGER_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <list>
#include <memory>
#include lt_tr1_functional
#include <torrent/common.h>

namespace torrent {

// Standard pair of up/down throttles.
// First element is upload throttle, second element is download throttle.
typedef std::pair<Throttle*, Throttle*> ThrottlePair;

// The sockaddr argument in the result call is NULL if the resolve failed,
// and the int holds the error code.
typedef std::function<void (const sockaddr*, int)> resolver_callback;

// Encapsulates whether we do genuine async resolution or fall back to sync.
// In a build with USE_UDNS, these do genuine asynchronous DNS resolution.
// In a build without it, they're stubbed out to use a synchronous getaddrinfo(3)
// call, while exposing the same API.
class LIBTORRENT_EXPORT AsyncResolver {
public:
  AsyncResolver(ConnectionManager *);

  // this queues a DNS resolve but doesn't send it. it doesn't execute any callbacks
  // and returns control immediately. the return value is an opaque identifier that
  // can be used to cancel the query (as long as the callback hasn't been executed yet):
  virtual void*   enqueue(const char *name, int family, resolver_callback *cbck) = 0;
  // this sends any queued resolves. it can execute arbitrary callbacks
  // before returning control:
  virtual void    flush() = 0;
  // this cancels a pending async query (as long as the callback hasn't executed yet):
  virtual void    cancel(void *query) = 0;
};

class LIBTORRENT_EXPORT ConnectionManager {
public:
  typedef uint32_t size_type;
  typedef uint16_t port_type;
  typedef uint8_t  priority_type;

  static const priority_type iptos_default     = 0;
  static const priority_type iptos_lowdelay    = IPTOS_LOWDELAY;
  static const priority_type iptos_throughput  = IPTOS_THROUGHPUT;
  static const priority_type iptos_reliability = IPTOS_RELIABILITY;

#if defined IPTOS_MINCOST
  static const priority_type iptos_mincost     = IPTOS_MINCOST;
#elif defined IPTOS_LOWCOST
  static const priority_type iptos_mincost     = IPTOS_LOWCOST;
#else
  static const priority_type iptos_mincost     = iptos_throughput;
#endif

  static const uint32_t encryption_none             = 0;
  static const uint32_t encryption_allow_incoming   = (1 << 0);
  static const uint32_t encryption_try_outgoing     = (1 << 1);
  static const uint32_t encryption_require          = (1 << 2);
  static const uint32_t encryption_require_RC4      = (1 << 3);
  static const uint32_t encryption_enable_retry     = (1 << 4);
  static const uint32_t encryption_prefer_plaintext = (1 << 5);

  // Internal to libtorrent.
  static const uint32_t encryption_use_proxy        = (1 << 6);
  static const uint32_t encryption_retrying         = (1 << 7);

  enum {
    handshake_incoming           = 1,
    handshake_outgoing           = 2,
    handshake_outgoing_encrypted = 3,
    handshake_outgoing_proxy     = 4,
    handshake_success            = 5,
    handshake_dropped            = 6,
    handshake_failed             = 7,
    handshake_retry_plaintext    = 8,
    handshake_retry_encrypted    = 9
  };

  typedef std::function<uint32_t (const sockaddr*)>     slot_filter_type;
  typedef std::function<ThrottlePair (const sockaddr*)> slot_throttle_type;

  typedef std::function<void (const char*, int, int, resolver_callback)> slot_resolver_type;

  ConnectionManager();
  ~ConnectionManager();
  
  // Check that we have not surpassed the max number of open sockets
  // and that we're allowed to connect to the socket address.
  //
  // Consider only checking max number of open sockets.
  bool                can_connect() const;

  // Call this to keep the socket count up to date.
  void                inc_socket_count()                      { m_size++; }
  void                dec_socket_count()                      { m_size--; }

  size_type           size() const                            { return m_size; }
  size_type           max_size() const                        { return m_maxSize; }

  priority_type       priority() const                        { return m_priority; }
  uint32_t            send_buffer_size() const                { return m_sendBufferSize; }
  uint32_t            receive_buffer_size() const             { return m_receiveBufferSize; }
  uint32_t            encryption_options()                    { return m_encryptionOptions; }

  void                set_max_size(size_type s)               { m_maxSize = s; }
  void                set_priority(priority_type p)           { m_priority = p; }
  void                set_send_buffer_size(uint32_t s);
  void                set_receive_buffer_size(uint32_t s);
  void                set_encryption_options(uint32_t options); 

  // Setting the addresses creates a copy of the address.
  const sockaddr*     bind_address() const                    { return m_bindAddress; }
  const sockaddr*     local_address() const                   { return m_localAddress; }
  const sockaddr*     proxy_address() const                   { return m_proxyAddress; }

  void                set_bind_address(const sockaddr* sa);
  void                set_local_address(const sockaddr* sa);
  void                set_proxy_address(const sockaddr* sa);

  uint32_t            filter(const sockaddr* sa);
  void                set_filter(const slot_filter_type& s)   { m_slot_filter = s; }

  bool                listen_open(port_type begin, port_type end);
  void                listen_close();  

  // Since trackers need our port number, it doesn't get cleared after
  // 'listen_close()'. The client may change the reported port number,
  // but do note that it gets overwritten after 'listen_open(...)'.
  port_type           listen_port() const                     { return m_listen_port; }
  int                 listen_backlog() const                  { return m_listen_backlog; }
  void                set_listen_port(port_type p)            { m_listen_port = p; }
  void                set_listen_backlog(int v);

  void*               enqueue_async_resolve(const char *name, int family, resolver_callback *cbck);
  void                flush_async_resolves();
  void                cancel_async_resolve(void *query);

  // Legacy synchronous resolver interface.
  slot_resolver_type& resolver()          { return m_slot_resolver; }

  // Asynchronous resolver interface.
  AsyncResolver&      async_resolver()    { return *m_async_resolver; }

  // The slot returns a ThrottlePair to use for the given address, or
  // NULLs to use the default throttle.
  slot_throttle_type& address_throttle()  { return m_slot_address_throttle; }

  // For internal usage.
  Listen*             listen()            { return m_listen; }

private:
  ConnectionManager(const ConnectionManager&);
  void operator = (const ConnectionManager&);

  size_type           m_size;
  size_type           m_maxSize;

  priority_type       m_priority;
  uint32_t            m_sendBufferSize;
  uint32_t            m_receiveBufferSize;
  int                 m_encryptionOptions;

  sockaddr*           m_bindAddress;
  sockaddr*           m_localAddress;
  sockaddr*           m_proxyAddress;

  Listen*             m_listen;
  port_type           m_listen_port;
  uint32_t            m_listen_backlog;

  slot_filter_type    m_slot_filter;
  slot_resolver_type  m_slot_resolver;
  slot_throttle_type  m_slot_address_throttle;

  std::unique_ptr<AsyncResolver> m_async_resolver;
};

}

#endif

