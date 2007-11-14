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

#ifndef LIBTORRENT_DOWNLOAD_CONNECTION_LIST_H
#define LIBTORRENT_DOWNLOAD_CONNECTION_LIST_H

#include <rak/functional.h>
#include <rak/socket_address.h>
#include <rak/unordered_vector.h>

namespace torrent {

class AddressList;
class Bitfield;
class DownloadMain;
class DownloadWrapper;
class Peer;
class PeerConnectionBase;
class PeerInfo;
class ProtocolExtension;
class SocketFd;
class EncryptionInfo;

class ConnectionList : private std::vector<Peer*> {
public:
  typedef std::vector<Peer*> base_type;
  typedef uint32_t           size_type;

  typedef rak::mem_fun1<DownloadWrapper, void, PeerConnectionBase*> slot_peer_type;

  typedef PeerConnectionBase* (*SlotNewConnection)(bool encrypted);

  using base_type::value_type;
  using base_type::reference;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::size;
  using base_type::empty;

  using base_type::front;
  using base_type::back;
  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;
  
  // Make sure any change here match PeerList's flags.
  static const int disconnect_available = (1 << 0);
  static const int disconnect_quick     = (1 << 1);
  static const int disconnect_unwanted  = (1 << 2);

  ConnectionList(DownloadMain* download) : m_minSize(50), m_maxSize(100), m_download(download) {}
  ~ConnectionList() { clear(); }

  // Does not do the usual cleanup done by 'erase'.
  void                clear();

  // Returns false if the connection was not added, the caller is then
  // responsible for cleaning up 'fd'.
  //
  // Clean this up, don't use this many arguments.
  PeerConnectionBase* insert(PeerInfo* p, const SocketFd& fd, Bitfield* bitfield, EncryptionInfo* encryptionInfo, ProtocolExtension* extensions);

  iterator            erase(iterator pos, int flags);
  void                erase(PeerInfo* peerInfo, int flags);
  void                erase(PeerConnectionBase* p, int flags);

  void                erase_remaining(iterator pos, int flags);
  void                erase_seeders();

  iterator            find(const char* id);
  iterator            find(const rak::socket_address& sa);

  size_type           get_min_size() const                              { return m_minSize; }
  void                set_min_size(size_type v)                         { m_minSize = v; }

  size_type           get_max_size() const                              { return m_maxSize; }
  void                set_max_size(size_type v);

  // Removes from 'l' addresses that are already connected to. Assumes
  // 'l' is sorted and unique.
  void                set_difference(AddressList* l);

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

  DownloadMain*       m_download;
};

}

#endif
