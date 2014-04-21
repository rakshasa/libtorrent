// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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
#include <tr1/functional>
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

  typedef std::tr1::function<DownloadMain* (const char*)> slot_download;

  // Do not connect to peers with this many or more failed chunks.
  static const unsigned int max_failed = 3;

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

  slot_download&      slot_download_id()         { return m_slot_download_id; }
  slot_download&      slot_download_obfuscated() { return m_slot_download_obfuscated; }

  // This needs to be filterable slot.
  DownloadMain*       download_info(const char* hash)                   { return m_slot_download_id(hash); }
  DownloadMain*       download_info_obfuscated(const char* hash)        { return m_slot_download_obfuscated(hash); }

  void                receive_succeeded(Handshake* h);
  void                receive_failed(Handshake* h, int message, int error);
  void                receive_timeout(Handshake* h);

  ProtocolExtension*  default_extensions() const                        { return &DefaultExtensions; }

private:
  void                create_outgoing(const rak::socket_address& sa, DownloadMain* info, int encryptionOptions);
  void                erase(Handshake* handshake);

  bool                setup_socket(SocketFd fd);

  static ProtocolExtension DefaultExtensions;

  slot_download       m_slot_download_id;
  slot_download       m_slot_download_obfuscated;
};

}

#endif
