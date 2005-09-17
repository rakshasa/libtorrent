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

#include <list>
#include <sigc++/signal.h>
#include <sigc++/slot.h>

#include "net/socket_address.h"
#include "torrent/peer.h"
#include "utils/unordered_vector.h"

namespace torrent {

class DownloadMain;
class PeerConnectionBase;
class PeerInfo;
class SocketFd;

class ConnectionList : private unordered_vector<PeerConnectionBase*> {
public:
  typedef unordered_vector<PeerConnectionBase*> Base;
  typedef std::list<SocketAddress>              AddressList;
  typedef uint32_t                              size_type;
  typedef sigc::signal1<void, Peer>             SignalPeer;
  typedef sigc::slot0<PeerConnectionBase*>      SlotNewConnection;

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
  
  ConnectionList() : m_minSize(50), m_maxSize(100) {}
  ~ConnectionList() { clear(); }

  // Does not do the usual cleanup done by 'erase'.
  void                clear();

  // Returns false if the connection was not added, the caller is then
  // responsible for cleaning up 'fd'.
  bool                insert(DownloadMain* d, const PeerInfo& p, const SocketFd& fd);

  iterator            erase(iterator pos);
  void                erase(PeerConnectionBase* p);
  void                erase_remaining(iterator pos);
  void                erase_seeders();

  iterator            find(const SocketAddress& sa);

  size_type           get_min_size() const                              { return m_minSize; }
  void                set_min_size(size_type v)                         { m_minSize = v; }

  size_type           get_max_size() const                              { return m_maxSize; }
  void                set_max_size(size_type v)                         { m_maxSize = v; }

  // Removes from 'l' addresses that are already connected to. Assumes
  // 'l' is sorted and unique.
  void                set_difference(AddressList* l);

  void                send_finished_chunk(uint32_t index);

  // When a peer is connected it should be removed from the list of
  // available peers.
  SignalPeer&         signal_connected()                               { return m_signalConnected; }

  // When a peer is disconnected the torrent should rebalance the
  // choked peers and connect to new ones if possible.
  SignalPeer&         signal_disconnected()                            { return m_signalDisconnected; }

  void                slot_new_connection(SlotNewConnection s)         { m_slotNewConnection = s; }

private:
  ConnectionList(const ConnectionList&);
  void operator = (const ConnectionList&);

  size_type           m_minSize;
  size_type           m_maxSize;

  SignalPeer          m_signalConnected;
  SignalPeer          m_signalDisconnected;

  SlotNewConnection   m_slotNewConnection;
};

}

#endif
