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

#ifndef LIBTORRENT_OBJECT_STATIC_MAP_H
#define LIBTORRENT_OBJECT_STATIC_MAP_H

#include <cstring>
#include <algorithm>
#include <torrent/object.h>

namespace torrent {

struct static_map_mapping_type {
  static const size_t max_key_size = 16;

  bool        is_end() const { return key[0] == '\0'; }
  static bool is_not_key_char(char c) { return c == '\0' || c == ':' || c == '[' || c == '*'; }

  const char* find_key_end(const char* pos) const { return std::find_if(pos, key + max_key_size, &is_not_key_char); }

  uint32_t    index;
  const char  key[max_key_size];
};
  
struct static_map_entry_type {
  torrent::Object object;
};

template <typename tmpl_key_type, size_t tmpl_length>
class static_map_type {
public:
  typedef Object                  value_type;
  typedef tmpl_key_type           key_type;
  typedef static_map_entry_type   entry_type;
  typedef static_map_mapping_type mapping_type;

  typedef mapping_type    key_list_type[tmpl_length];
  typedef entry_type      value_list_type[tmpl_length];

  static const size_t size = tmpl_length;
  static const key_list_type keys;

  entry_type*         values() { return m_values; }
  const entry_type*   values() const { return m_values; }

  Object&             operator [] (key_type key)        { return m_values[key].object; }
  const Object&       operator [] (key_type key) const  { return m_values[key].object; }

private:
  value_list_type m_values;
};

//
// Helper functions/classes for parsing keys:
//

struct static_map_stack_type {
  void set_key_index(uint32_t start_index, uint32_t end_index, uint32_t delim_size,
                     torrent::Object::type_type type = Object::TYPE_MAP) {
    key_index = start_index;
    next_key = end_index + delim_size;
    obj_type = type;
  }

  void clear() { key_index = 0; next_key = 0; obj_type = Object::TYPE_MAP; }

  uint32_t key_index;
  uint32_t next_key;
  Object::type_type obj_type;
};

typedef std::pair<const static_map_mapping_type*, unsigned int> static_map_key_search_result;

// Note that the key for both functions must be null-terminated at
// 'key_last'.
const static_map_key_search_result
find_key_match(const static_map_mapping_type* first, const static_map_mapping_type* last,
               const char* key_first, const char* key_last) LIBTORRENT_EXPORT;

inline const static_map_key_search_result
find_key_match(const static_map_mapping_type* first, const static_map_mapping_type* last,
               const char* key) {
  return find_key_match(first, last, key, key + strlen(key));
}

}

#endif
