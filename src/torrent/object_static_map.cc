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

#include <rak/algorithm.h>

#include "object_static_map.h"

namespace torrent {

const static_map_key_search_result
find_key_match(const static_map_mapping_type* first, const static_map_mapping_type* last,
               const char* key_first, const char* key_last) {
  //  unsigned int key_length = strlen(key);
  const static_map_mapping_type* itr = first;

  while (itr != last) {
    unsigned int base = rak::count_base(key_first, key_last, itr->key, itr->key + itr->max_key_size);

    if (key_first[base] != '\0') {
      // Return not found here if we know the entry won't come after
      // this.
      itr++;
      continue;
    }

    if (itr->key[base] == '\0' || itr->key[base] == '*' ||
        (itr->key[base] == ':' && itr->key[base + 1] == ':') ||
        (itr->key[base] == '[' && itr->key[base + 1] == ']'))
      return static_map_key_search_result(itr, base);

    break;
  }

  return static_map_key_search_result(first, 0);
}

}
