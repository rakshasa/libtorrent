#include <cstring>
#include "torrent/object.h"

torrent::Object create_bencode(const char* str);
torrent::Object create_bencode_c(const char* str);

inline torrent::Object create_bencode_raw_bencode_c(const char* str) { return torrent::raw_bencode(str, std::strlen(str)); }
inline torrent::Object create_bencode_raw_string_c(const char* str) { return torrent::raw_string(str, std::strlen(str)); }
inline torrent::Object create_bencode_raw_list_c(const char* str) { return torrent::raw_list(str, std::strlen(str)); }
inline torrent::Object create_bencode_raw_map_c(const char* str) { return torrent::raw_map(str, std::strlen(str)); }

bool validate_bencode(const char* first, const char* last);

bool compare_bencode(const torrent::Object& obj, const char* str, uint32_t skip_mask = 0);
