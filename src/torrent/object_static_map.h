#ifndef LIBTORRENT_OBJECT_STATIC_MAP_H
#define LIBTORRENT_OBJECT_STATIC_MAP_H

#include <cstring>
#include <algorithm>
#include <torrent/object.h>

namespace torrent {

struct static_map_mapping_type {
  static constexpr size_t max_key_size = 16;

  bool        is_end() const { return key[0] == '\0'; }
  static bool is_not_key_char(char c) { return c == '\0' || c == ':' || c == '[' || c == '*'; }

  const char* find_key_end(const char* pos) const { return std::find_if(pos, key + max_key_size, &is_not_key_char); }

  uint32_t    index;
  char        key[max_key_size];
};

struct static_map_entry_type {
  torrent::Object object;
};

template <typename tmpl_key_type, size_t tmpl_length>
class static_map_type {
public:
  using value_type   = Object;
  using key_type     = tmpl_key_type;
  using entry_type   = static_map_entry_type;
  using mapping_type = static_map_mapping_type;

  typedef mapping_type    key_list_type[tmpl_length];
  typedef entry_type      value_list_type[tmpl_length];

  static constexpr size_t size = tmpl_length;
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

using static_map_key_search_result = std::pair<const static_map_mapping_type*, unsigned int>;

// Note that the key for both functions must be null-terminated at
// 'key_last'.
static_map_key_search_result
find_key_match(const static_map_mapping_type* first, const static_map_mapping_type* last,
               const char* key_first, const char* key_last) LIBTORRENT_EXPORT;

inline static_map_key_search_result
find_key_match(const static_map_mapping_type* first, const static_map_mapping_type* last,
               const char* key) {
  return find_key_match(first, last, key, key + strlen(key));
}

} // namespace torrent

#endif
