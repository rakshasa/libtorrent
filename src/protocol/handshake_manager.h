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
#include <torrent/connection_manager.h>

#include "net/socket_fd.h"

namespace torrent {

class Handshake;
class DownloadManager;
class DownloadMain;
class PeerConnectionBase;

class HandshakeManager : private rak::unordered_vector<Handshake*> {
public:
  typedef rak::unordered_vector<Handshake*> base_type;
  typedef uint32_t                          size_type;

  typedef rak::mem_fun1<DownloadManager, DownloadMain*, const char*> SlotDownloadId;

  using base_type::empty;

  HandshakeManager() { }
  ~HandshakeManager() { clear(); }

  size_type           size() const { return base_type::size(); }
  size_type           size_info(DownloadMain* info) const;

  void                clear();

  bool                find(const rak::socket_address& sa);

  void                erase_download(DownloadMain* info);

  // Cleanup.
  void                add_incoming(SocketFd fd, const rak::socket_address& sa);
  void                add_outgoing(const rak::socket_address& sa, DownloadMain* info);

  void                slot_download_id(SlotDownloadId s)                { m_slotDownloadId = s; }
  void                slot_download_id_obfuscated(SlotDownloadId s)     { m_slotDownloadIdObfuscated = s; }

  void                receive_succeeded(Handshake* h);
  void                receive_failed(Handshake* h, ConnectionManager::HandshakeMessage message, uint32_t err);
  void                receive_timeout(Handshake* h);

  // This needs to be filterable slot.
  DownloadMain*       download_info(const char* hash)                   { return m_slotDownloadId(hash); }
  DownloadMain*       download_info_obfuscated(const char* hash)        { return m_slotDownloadIdObfuscated(hash); }

private:
  void                create_outgoing(const rak::socket_address& sa, DownloadMain* info, int encryptionOptions);
  void                erase(Handshake* handshake);

  bool                setup_socket(SocketFd fd);

  SlotDownloadId      m_slotDownloadId;
  SlotDownloadId      m_slotDownloadIdObfuscated;
};

}

#endif
