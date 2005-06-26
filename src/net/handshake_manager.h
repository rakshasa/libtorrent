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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_NET_HANDSHAKE_MANAGER_H
#define LIBTORRENT_NET_HANDSHAKE_MANAGER_H

#include <list>
#include <string>
#include <inttypes.h>
#include <sigc++/slot.h>

#include "socket_address.h"
#include "socket_fd.h"

namespace torrent {

class PeerInfo;
class Handshake;

class HandshakeManager {
public:
  typedef std::list<Handshake*> HandshakeList;

  HandshakeManager() : m_size(0) { m_bindAddress.set_address_any(); }
  ~HandshakeManager() { clear(); }

  void                add_incoming(int fd,
				   const std::string& dns,
				   uint16_t port);

  void                add_outgoing(const PeerInfo& p,
				   const std::string& infoHash,
				   const std::string& ourId);

  void                clear();

  uint32_t            get_size()                                { return m_size; }
  uint32_t            get_size_hash(const std::string& hash);

  void                set_bind_address(const SocketAddress& sa) { m_bindAddress = sa; }

  bool                has_peer(const PeerInfo& p);

  // File descriptor
  // Info hash
  // Peer info
  typedef sigc::slot3<void, SocketFd, const std::string&, const PeerInfo&> SlotConnected;
  typedef sigc::slot1<std::string, const std::string&>                     SlotDownloadId;

  void                slot_connected(SlotConnected s)           { m_slotConnected = s; }
  void                slot_download_id(SlotDownloadId s)        { m_slotDownloadId = s; }

  void                receive_connected(Handshake* h);
  void                receive_failed(Handshake* h);

  std::string         get_download_id(const std::string& hash)  { return m_slotDownloadId(hash); }

private:

  void                remove(Handshake* h);

  SocketFd            make_socket(SocketAddress& sa);

  HandshakeList       m_handshakes;
  uint32_t            m_size;

  SlotConnected       m_slotConnected;
  SlotDownloadId      m_slotDownloadId;

  SocketAddress       m_bindAddress;
};

}

#endif
