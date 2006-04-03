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

// Add some helpfull words here.

#ifndef LIBTORRENT_CONNECTION_MANAGER_H
#define LIBTORRENT_CONNECTION_MANAGER_H

#include <inttypes.h>
#include <netinet/ip.h>
#include <sigc++/slot.h>

class sockaddr;

namespace torrent {

// Internal.
class Listen;

class ConnectionManager {
public:
  typedef uint32_t                              size_type;
  typedef uint16_t                              port_type;
  typedef uint8_t                               priority_type;
  typedef sigc::slot<uint32_t, const sockaddr*> slot_filter_type;

  static const priority_type iptos_default     = 0;
  static const priority_type iptos_lowdelay    = IPTOS_LOWDELAY;
  static const priority_type iptos_throughput  = IPTOS_THROUGHPUT;
  static const priority_type iptos_reliability = IPTOS_RELIABILITY;
  static const priority_type iptos_mincost     = IPTOS_MINCOST;

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
  void                set_max_size(size_type s)               { m_maxSize = s; }

  priority_type       priority() const                        { return m_priority; }
  void                set_priority(priority_type p)           { m_priority = p; }

  uint32_t            send_buffer_size() const                { return m_sendBufferSize; }
  void                set_send_buffer_size(uint32_t s);

  uint32_t            receive_buffer_size() const             { return m_receiveBufferSize; }
  void                set_receive_buffer_size(uint32_t s);

  // Propably going to have to make m_bindAddress a pointer to make it
  // safe.
  //
  // Perhaps add length parameter.
  //
  // Setting the bind address makes a copy.
  const sockaddr*     bind_address() const                    { return m_bindAddress; }
  void                set_bind_address(const sockaddr* sa);

  const sockaddr*     local_address() const                   { return m_localAddress; }
  void                set_local_address(const sockaddr* sa);

  uint32_t            filter(const sockaddr* sa);
  void                set_filter(const slot_filter_type& s)   { m_slotFilter = s; }

  bool                listen_open(port_type begin, port_type end);
  void                listen_close();  

  // Since trackers need our port number, it doesn't get cleared after
  // 'listen_close()'. The client may change the reported port number,
  // but do note that it gets overwritten after 'listen_open(...)'.
  port_type           listen_port() const                     { return m_listenPort; }
  void                set_listen_port(port_type p)            { m_listenPort = p; }

  // For internal usage.
  Listen*             listen()                                { return m_listen; }

private:
  ConnectionManager(const ConnectionManager&);
  void operator = (const ConnectionManager&);

  size_type           m_size;
  size_type           m_maxSize;

  priority_type       m_priority;
  uint32_t            m_sendBufferSize;
  uint32_t            m_receiveBufferSize;

  sockaddr*           m_bindAddress;
  sockaddr*           m_localAddress;

  Listen*             m_listen;
  port_type           m_listenPort;

  slot_filter_type    m_slotFilter;
};

}

#endif

