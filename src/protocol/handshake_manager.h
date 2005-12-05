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

#ifndef LIBTORRENT_NET_HANDSHAKE_MANAGER_H
#define LIBTORRENT_NET_HANDSHAKE_MANAGER_H

#include <list>
#include <string>
#include <inttypes.h>
#include <rak/functional.h>

#include "net/socket_address.h"
#include "net/socket_fd.h"

namespace torrent {

class Handshake;
class PeerInfo;
class Manager;

class HandshakeManager {
public:
  typedef std::list<Handshake*> HandshakeList;

  // File descriptor
  // Info hash
  // Peer info
  typedef rak::mem_fun3<Manager, void, SocketFd, const std::string&, const PeerInfo&> SlotConnected;
  typedef rak::mem_fun1<Manager, std::string, const std::string&>                     SlotDownloadId;

  HandshakeManager() : m_size(0) { m_bindAddress.set_address_any(); }
  ~HandshakeManager() { clear(); }

  void                add_incoming(SocketFd fd, const SocketAddress& sa);

  void                add_outgoing(const SocketAddress& sa,
				   const std::string& infoHash,
				   const std::string& ourId);

  void                clear();

  uint32_t            size()                                { return m_size; }
  uint32_t            size_hash(const std::string& hash);

  void                set_bind_address(const SocketAddress& sa) { m_bindAddress = sa; }

  bool                has_address(const SocketAddress& sa);

  void                slot_connected(SlotConnected s)           { m_slotConnected = s; }
  void                slot_download_id(SlotDownloadId s)        { m_slotDownloadId = s; }

  void                receive_connected(Handshake* h);
  void                receive_failed(Handshake* h);

  std::string         get_download_id(const std::string& hash)  { return m_slotDownloadId(hash); }

private:

  void                remove(Handshake* h);

  HandshakeList       m_handshakes;
  uint32_t            m_size;

  SlotConnected       m_slotConnected;
  SlotDownloadId      m_slotDownloadId;

  SocketAddress       m_bindAddress;
};

}

#endif
