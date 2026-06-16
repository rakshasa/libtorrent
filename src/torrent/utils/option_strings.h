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
  OPTION_SOCKET_CATEGORY,
  OPTION_TRACKER_EVENT,

  OPTION_MAX_SIZE,
  OPTION_START_COMPACT = OPTION_HANDSHAKE_CONNECTION,
  OPTION_SINGLE_SIZE = OPTION_MAX_SIZE - OPTION_START_COMPACT
};

int             option_find_string(option_enum opt_enum, const char* name) LIBTORRENT_EXPORT;
inline int      option_find_string_str(option_enum opt_enum, const std::string& name) { return option_find_string(opt_enum, name.c_str()); }

const char*     option_to_c_str(option_enum opt_enum, unsigned int value, const char* not_found = "invalid") LIBTORRENT_EXPORT;
const char*     option_to_c_str_or_throw(option_enum opt_enum, unsigned int value, const char* not_found = "Invalid option value") LIBTORRENT_EXPORT;

std::string     option_to_str(option_enum opt_enum, unsigned int value, const char* not_found = "invalid");
std::string     option_to_str_or_throw(option_enum opt_enum, unsigned int value, const char* not_found = "Invalid option value");

torrent::Object option_list_strings(option_enum opt_enum) LIBTORRENT_EXPORT;

//
// Implementation:
//

inline std::string
option_to_str(option_enum opt_enum, unsigned int value, const char* not_found) {
  return option_to_c_str(opt_enum, value, not_found);
}

inline std::string
option_to_str_or_throw(option_enum opt_enum, unsigned int value, const char* not_found) {
  return option_to_c_str_or_throw(opt_enum, value, not_found);
}

} // namespace torrent

#endif
