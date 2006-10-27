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

// Handshake errors passed to signal handler
const int EH_None                    = 0;
const int EH_NotBTProtocol           = 1;
const int EH_NotAcceptingPeers       = 2;
const int EH_Duplicate               = 3;
const int EH_Unknown                 = 4;
const int EH_Inactive                = 5;
const int EH_SeederRejected          = 6;
const int EH_IsSelf                  = 7;
const int EH_InvalidValue            = 8;
const int EH_UnencryptedRejected     = 9;
const int EH_InvalidEncryptionMethod = 10;
const int EH_EncryptionSyncFailed    = 11;
const int EH_NetworkError            = 12;

const int E_Last                     = 12;

const char* strerror(int err) LIBTORRENT_EXPORT;

}

#endif
