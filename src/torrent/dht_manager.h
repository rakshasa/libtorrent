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

// Add some helpfull words here.

#ifndef LIBTORRENT_DHT_MANAGER_H
#define LIBTORRENT_DHT_MANAGER_H

#include <torrent/common.h>
#include <torrent/connection_manager.h>

namespace torrent {

class ThrottleList;

class LIBTORRENT_EXPORT DhtManager {
public:
  typedef ConnectionManager::port_type          port_type;

  struct statistics_type {
    // Cycle; 0=inactive, 1=initial bootstrapping, 2 and up=normal operation
    unsigned int       cycle;

    // UDP transfer rates.
    const Rate&        up_rate;
    const Rate&        down_rate;

    // DHT query statistics.
    unsigned int       queries_received;
    unsigned int       queries_sent;
    unsigned int       replies_received;

    // DHT node info.
    unsigned int       num_nodes;
    unsigned int       num_buckets;

    // DHT tracker info.
    unsigned int       num_peers;
    unsigned int       max_peers;
    unsigned int       num_trackers;

    statistics_type(const Rate& up, const Rate& down) : up_rate(up), down_rate(down) { }
  };

  DhtManager() : m_router(NULL), m_portSent(0), m_canReceive(true) { };
  ~DhtManager();

  void                initialize(const Object& dhtCache);

  void                start(port_type port);
  void                stop();

  // Store DHT cache in the given container and return the container.
  Object*             store_cache(Object* container) const;

  bool                is_valid() const                        { return m_router; }
  bool                is_active() const;

  port_type           port() const                            { return m_port; }

  bool                can_receive_queries() const             { return m_canReceive; }

  // Call this after sending the port to a client, so the router knows
  // that it should be getting requests.
  void                port_sent()                             { m_portSent++; }

  // Add a node by host (from a torrent file), or by address from explicit add_node
  // command or the BT PORT message.
  void                add_node(const std::string& host, int port);
  void                add_node(const sockaddr* addr, int port);

  statistics_type     get_statistics() const;
  void                reset_statistics();

  // To be called if upon examining the statistics, the client decides that
  // we can't receive outside requests and therefore shouldn't advertise our
  // UDP port after the BT handshake.
  void                set_can_receive(bool can)               { m_canReceive = can; }

  // Internal libTorrent use only
  DhtRouter*          router()                                { return m_router; }

private:
  DhtRouter*          m_router;
  port_type           m_port;

  int                 m_portSent;
  bool                m_canReceive;
};

}

#endif
