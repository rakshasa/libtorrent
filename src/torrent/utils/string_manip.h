#ifndef LIBTORRENT_TORRENT_UTILS_STRING_MANIP_H
#define LIBTORRENT_TORRENT_UTILS_STRING_MANIP_H

#include <string>
#include <torrent/common.h>

namespace torrent::utils {

// TODO: Deprecate trim_string and sanitize_string.

std::string trim_string(const std::string& str) LIBTORRENT_EXPORT;

std::string string_from_hex_or_empty(const char* hex_begin, const char* hex_end) LIBTORRENT_EXPORT;
bool        string_from_hex_with_check(const char* hex_begin, const char* hex_end, char* dest_begin, char* dest_end) LIBTORRENT_EXPORT;

std::string string_to_hex(const char* begin, const char* end) LIBTORRENT_EXPORT;
char*       string_to_hex(const char* begin, const char* end, char* dest) LIBTORRENT_EXPORT;

std::string string_with_escape_codes(const std::string& str) LIBTORRENT_EXPORT;

std::string sanitize_string(const std::string& str) LIBTORRENT_EXPORT;
std::string sanitize_string_with_escape_codes(const std::string& str) LIBTORRENT_EXPORT;
std::string sanitize_string_with_tags(const std::string& str) LIBTORRENT_EXPORT;

} // namespace torrent::utils

#endif // LIBTORRENT_TORRENT_UTILS_STRING_MANIP_H
