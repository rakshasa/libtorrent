// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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

#ifndef LIBTORRENT_DOWNLOAD_CONNECTION_LIST_H
#define LIBTORRENT_DOWNLOAD_CONNECTION_LIST_H

#include <sigc++/signal.h>
#include <sigc++/slot.h>

#include "net/socket_fd.h"
#include "torrent/peer.h"
#include "utils/unordered_vector.h"

namespace torrent {

class PeerConnectionBase;
class PeerInfo;

class ConnectionList : private unordered_vector<PeerConnectionBase*> {
public:
  typedef unordered_vector<PeerConnectionBase*> Base;

  using Base::value_type;
  using Base::reference;

  using Base::iterator;
  using Base::reverse_iterator;
  using Base::size;
  using Base::empty;

  using Base::front;
  using Base::back;
  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;
  
  ConnectionList() : m_maxConnections(100) {}
  ~ConnectionList() { clear(); }

  // Does not do the usual cleanup done by 'erase'.
  void                clear();

  // Returns false if the connection was not added, the caller is then
  // responsible for cleaning up 'fd'.
  bool                insert(SocketFd fd, const PeerInfo& p);
  void                erase(PeerConnectionBase* p);

  uint32_t            get_max_connections() const                      { return m_maxConnections; }
  void                set_max_connections(uint32_t v)                  { m_maxConnections = v; }

  typedef sigc::signal1<void, Peer>                                    SignalPeer;
  typedef sigc::slot2<PeerConnectionBase*, SocketFd, const PeerInfo&>  SlotNewConnection;

  // When a peer is connected it should be removed from the list of
  // available peers.
  SignalPeer&         signal_peer_connected()                          { return m_signalPeerConnected; }

  // When a peer is disconnected the torrent should rebalance the
  // choked peers and connect to new ones if possible.
  SignalPeer&         signal_peer_disconnected()                       { return m_signalPeerDisconnected; }

  void                slot_new_connection(SlotNewConnection s)         { m_slotNewConnection = s; }

private:
  ConnectionList(const ConnectionList&);
  void operator = (const ConnectionList&);

  uint32_t            m_maxConnections;

  SignalPeer          m_signalPeerConnected;
  SignalPeer          m_signalPeerDisconnected;

  SlotNewConnection   m_slotNewConnection;
};

}

#endif
