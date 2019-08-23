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

#include "config.h"

#include "error.h"

namespace torrent {

static const char* errorStrings[e_last + 1] = {
  "unknown error",                      // e_none

  "not BitTorrent protocol",            // eh_not_bittorrent
  "not accepting connections",          // eh_not_accepting_connections
  "duplicate peer ID",                  // eh_duplicate
  "unknown download",                   // eh_unknown_download
  "download inactive",                  // eh_inactive_download
  "seeder rejected",                    // eh_unwanted_connection
  "is self",                            // eh_is_self
  "invalid value received",             // eh_invalid_value
  "unencrypted connection rejected",    // eh_unencrypted_rejected
  "invalid encryption method",          // eh_invalid_encryption
  "encryption sync failed",             // eh_encryption_sync_failed
  "<deprecated>",                       // eh_
  "network unreachable",                // eh_network_unreachable
  "network timeout",                    // eh_network_timeout
  "invalid message order",              // eh_invalid_order
  "too many failed chunks",             // eh_toomanyfailed
  "no peer info",                       // eh_no_peer_info
  "network socket error",               // eh_network_socket_error
  "network read error",                 // eh_network_read_error
  "network write error",                // eh_network_write_error

//   "", // e_handshake_incoming
//   "", // e_handshake_outgoing
//   "", // e_handshake_outgoing_encrypted
//   "", // e_handshake_outgoing_proxy
//   "", // e_handshake_success
//   "", // e_handshake_retry_plaintext
//   ""  // e_handshake_retry_encrypted
};

const char*
strerror(int err) {
  if (err < 0 || err > e_last)
    return "Unknown error";
  
  return errorStrings[err];
}

}
