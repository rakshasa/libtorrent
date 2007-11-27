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

#ifndef LIBTORRENT_PEER_CONNECTION_LIST_H
#define LIBTORRENT_PEER_CONNECTION_LIST_H

#include <vector>
#include <sigc++/signal.h>
#include <torrent/common.h>

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
class HandshakeManager;

class LIBTORRENT_EXPORT ConnectionList : private std::vector<Peer*> {
public:
  friend class DownloadMain;
  friend class DownloadWrapper;
  friend class HandshakeManager;

  typedef std::vector<Peer*>         base_type;
  typedef uint32_t                   size_type;
  typedef sigc::signal1<void, Peer*> signal_peer_type;

  typedef PeerConnectionBase* (*slot_new_conn_type)(bool encrypted);

  using base_type::value_type;
  using base_type::reference;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::reverse_iterator;
  using base_type::const_iterator;
  using base_type::const_reverse_iterator;
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

  ConnectionList(DownloadMain* download);

  // Make these protected?
  iterator            erase(iterator pos, int flags);
  void                erase(PeerInfo* peerInfo, int flags);
  void                erase(PeerConnectionBase* p, int flags);

  void                erase_remaining(iterator pos, int flags);
  void                erase_seeders();

  iterator            find(const char* id);
  iterator            find(const sockaddr* sa);

  size_type           min_size() const                          { return m_minSize; }
  void                set_min_size(size_type v);

  size_type           max_size() const                          { return m_maxSize; }
  void                set_max_size(size_type v);

  // Removes from 'l' addresses that are already connected to. Assumes
  // 'l' is sorted and unique.
  void                set_difference(AddressList* l);

  signal_peer_type&   signal_connected()                        { return m_signalConnected; }
  signal_peer_type&   signal_disconnected()                     { return m_signalDisconnected; }

  // Move to protected:
  void                slot_new_connection(slot_new_conn_type s) { m_slotNewConnection = s; }

protected:
  // Does not do the usual cleanup done by 'erase'.
  void                clear() LIBTORRENT_NO_EXPORT;

  // Returns false if the connection was not added, the caller is then
  // responsible for cleaning up 'fd'.
  //
  // Clean this up, don't use this many arguments.
  PeerConnectionBase* insert(PeerInfo* p, const SocketFd& fd, Bitfield* bitfield, EncryptionInfo* encryptionInfo, ProtocolExtension* extensions) LIBTORRENT_NO_EXPORT;

private:
  ConnectionList(const ConnectionList&) LIBTORRENT_NO_EXPORT;
  void operator = (const ConnectionList&) LIBTORRENT_NO_EXPORT;

  DownloadMain*       m_download;

  size_type           m_minSize;
  size_type           m_maxSize;

  signal_peer_type    m_signalConnected;
  signal_peer_type    m_signalDisconnected;

  slot_new_conn_type  m_slotNewConnection;
};

}

#endif
