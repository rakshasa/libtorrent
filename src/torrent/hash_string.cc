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

#include <cctype>
#include <rak/string_manip.h>

#include "hash_string.h"

namespace torrent {

const char*
hash_string_from_hex_c_str(const char* first, HashString& hash) {
  const char* hash_first = first;

  torrent::HashString::iterator itr = hash.begin();
  
  while (itr != hash.end()) {
    if (!std::isxdigit(*first) || !std::isxdigit(*(first + 1)))
      return hash_first;

    *itr++ = (rak::hexchar_to_value(*first) << 4) + rak::hexchar_to_value(*(first + 1));
    first += 2;
  }

  return first;
}
  
char*
hash_string_to_hex(const HashString& hash, char* first) {
  return rak::transform_hex(hash.begin(), hash.end(), first);
}

std::string
hash_string_to_hex_str(const HashString& hash) {
  std::string str(HashString::size_data * 2, '\0');
  rak::transform_hex(hash.begin(), hash.end(), str.begin());

  return str;
}

}
