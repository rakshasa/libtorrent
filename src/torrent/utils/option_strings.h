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

#ifndef LIBTORRENT_UTILS_OPTION_STRINGS_H
#define LIBTORRENT_UTILS_OPTION_STRINGS_H

#include <string>
#include <torrent/common.h>

namespace torrent {

class Object;

enum option_enum {
  OPTION_CONNECTION_TYPE,
  OPTION_CHOKE_HEURISTICS,
  OPTION_CHOKE_HEURISTICS_DOWNLOAD,
  OPTION_CHOKE_HEURISTICS_UPLOAD,
  OPTION_ENCRYPTION,
  OPTION_IP_FILTER,
  OPTION_IP_TOS,
  OPTION_TRACKER_MODE,

  OPTION_HANDSHAKE_CONNECTION,
  OPTION_LOG_GROUP,
  OPTION_TRACKER_EVENT,

  OPTION_MAX_SIZE,
  OPTION_START_COMPACT = OPTION_HANDSHAKE_CONNECTION,
  OPTION_SINGLE_SIZE = OPTION_MAX_SIZE - OPTION_START_COMPACT
};

int             option_find_string(option_enum opt_enum, const char* name) LIBTORRENT_EXPORT;
inline int      option_find_string_str(option_enum opt_enum, const std::string& name) { return option_find_string(opt_enum, name.c_str()); }

const char*     option_to_string(option_enum opt_enum, unsigned int value, const char* not_found = "invalid") LIBTORRENT_EXPORT;
const char*     option_to_string_or_throw(option_enum opt_enum, unsigned int value, const char* not_found = "Invalid option value") LIBTORRENT_EXPORT;

// TODO: Deprecated.
const char*     option_as_string(option_enum opt_enum, unsigned int value) LIBTORRENT_EXPORT;

torrent::Object option_list_strings(option_enum opt_enum) LIBTORRENT_EXPORT;

}

#endif
