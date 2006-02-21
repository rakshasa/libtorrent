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

#ifndef LIBTORRENT_NET_HANDSHAKE_MANAGER_H
#define LIBTORRENT_NET_HANDSHAKE_MANAGER_H

#include <string>
#include <inttypes.h>
#include <rak/functional.h>
#include <rak/unordered_vector.h>
#include <rak/socket_address.h>

#include "net/socket_fd.h"

namespace torrent {

class Handshake;
class PeerInfo;
class Manager;
class DownloadManager;
class TrackerInfo;

class HandshakeManager : private rak::unordered_vector<Handshake*> {
public:
  typedef rak::unordered_vector<Handshake*> Base;
  typedef uint32_t                          size_type;

  typedef rak::mem_fun3<Manager, void, SocketFd, TrackerInfo*, const PeerInfo&> SlotConnected;
  typedef rak::mem_fun1<DownloadManager, TrackerInfo*, const std::string&>      SlotDownloadId;

  using Base::empty;

  HandshakeManager() { m_bindAddress.set_family(); }
  ~HandshakeManager() { clear(); }

  size_type           size() const { return Base::size(); }
  size_type           size_info(TrackerInfo* info) const;

  void                clear();

  bool                find(const rak::socket_address& sa);

  // Cleanup.
  void                add_incoming(SocketFd fd, const rak::socket_address& sa);
  void                add_outgoing(const rak::socket_address& sa, TrackerInfo* info);

  void                set_bind_address(const rak::socket_address& sa) { m_bindAddress = sa; }

  void                slot_connected(SlotConnected s)           { m_slotConnected = s; }
  void                slot_download_id(SlotDownloadId s)        { m_slotDownloadId = s; }

  void                receive_connected(Handshake* h);
  void                receive_failed(Handshake* h);

  // This needs to be filterable slot.
  TrackerInfo*        download_info(const std::string& hash)    { return m_slotDownloadId(hash); }

private:
  void                erase(Handshake* handshake);

  SlotConnected       m_slotConnected;
  SlotDownloadId      m_slotDownloadId;

  rak::socket_address m_bindAddress;
};

}

#endif
