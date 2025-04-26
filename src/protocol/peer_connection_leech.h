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

#ifndef LIBTORRENT_PROTOCOL_PEER_CONNECTION_LEECH_H
#define LIBTORRENT_PROTOCOL_PEER_CONNECTION_LEECH_H

#include "peer_connection_base.h"

#include "torrent/download.h"

namespace torrent {

// Type-specific data.
template<Download::ConnectionType type> struct PeerConnectionData;

template<> struct PeerConnectionData<Download::CONNECTION_LEECH> { };

template<> struct PeerConnectionData<Download::CONNECTION_SEED> { };

template<> struct PeerConnectionData<Download::CONNECTION_INITIAL_SEED> {
  PeerConnectionData() : lastIndex(~uint32_t()) { }
  uint32_t lastIndex;
  uint32_t bytesLeft;
};

template<Download::ConnectionType type>
class PeerConnection : public PeerConnectionBase {
public:
  PeerConnection() = default;
  ~PeerConnection() override;

  void                initialize_custom() override;
  void                update_interested() override;
  bool                receive_keepalive() override;

  void                event_read() override;
  void                event_write() override;

private:
  PeerConnection(const PeerConnection&) = delete;
  PeerConnection& operator=(const PeerConnection&) = delete;

  inline bool         read_message();
  void                read_have_chunk(uint32_t index);

  void                offer_chunk();
  bool                should_upload();

  inline void         fill_write_buffer();

  PeerConnectionData<type> m_data;
};

}

#endif
