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

#ifndef LIBTORRENT_DOWNLOAD_CONNECTION_LIST_H
#define LIBTORRENT_DOWNLOAD_CONNECTION_LIST_H

#include <list>
#include <rak/functional.h>
#include <rak/socket_address.h>
#include <rak/unordered_vector.h>

namespace torrent {

class DownloadMain;
class DownloadWrapper;
class PeerConnectionBase;
class PeerInfo;
class SocketFd;

class ConnectionList : private rak::unordered_vector<PeerConnectionBase*> {
public:
  typedef rak::unordered_vector<PeerConnectionBase*> Base;
  typedef std::list<rak::socket_address>             AddressList;
  typedef uint32_t                                   size_type;

  typedef rak::mem_fun1<DownloadWrapper, void, PeerConnectionBase*> slot_peer_type;

  typedef PeerConnectionBase* (*SlotNewConnection)();

  using Base::value_type;
  using Base::reference;
  using Base::difference_type;

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

  iterator            find(const rak::socket_address& sa);

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
  void                slot_connected(slot_peer_type s)                 { m_slotConnected = s; }

  // When a peer is disconnected the torrent should rebalance the
  // choked peers and connect to new ones if possible.
  void                slot_disconnected(slot_peer_type s)              { m_slotDisconnected = s; }

  void                slot_new_connection(SlotNewConnection s)         { m_slotNewConnection = s; }

private:
  ConnectionList(const ConnectionList&);
  void operator = (const ConnectionList&);

  size_type           m_minSize;
  size_type           m_maxSize;

  slot_peer_type      m_slotConnected;
  slot_peer_type      m_slotDisconnected;

  SlotNewConnection   m_slotNewConnection;
};

}

#endif
