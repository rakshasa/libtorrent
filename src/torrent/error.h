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

#ifndef LIBTORRENT_TORRENT_ERROR_H
#define LIBTORRENT_TORRENT_ERROR_H

#include <torrent/common.h>

namespace torrent {

const int e_none                                = 0;

// Handshake errors passed to signal handler
const int e_handshake_not_bittorrent            = 1;
const int e_handshake_not_accepting_connections = 2;
const int e_handshake_duplicate                 = 3;
const int e_handshake_unknown_download          = 4;
const int e_handshake_inactive_download         = 5;
const int e_handshake_unwanted_connection       = 6;
const int e_handshake_is_self                   = 7;
const int e_handshake_invalid_value             = 8;
const int e_handshake_unencrypted_rejected      = 9;
const int e_handshake_invalid_encryption        = 10;
const int e_handshake_encryption_sync_failed    = 11;
const int e_handshake_network_error             = 12;

// const int e_handshake_incoming                  = 13;
// const int e_handshake_outgoing                  = 14;
// const int e_handshake_outgoing_encrypted        = 15;
// const int e_handshake_outgoing_proxy            = 16;
// const int e_handshake_success                   = 17;
// const int e_handshake_retry_plaintext           = 18;
// const int e_handshake_retry_encrypted           = 19;

const int e_last                                = 12;

const char* strerror(int err) LIBTORRENT_EXPORT;

}

#endif
