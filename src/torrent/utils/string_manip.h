#ifndef LIBTORRENT_TORRENT_UTILS_STRING_MANIP_H
#define LIBTORRENT_TORRENT_UTILS_STRING_MANIP_H

#include <string>
#include <torrent/common.h>

namespace torrent::utils {

// TODO: Deprecate trim_string and sanitize_string.

std::string trim_string(const std::string& str) LIBTORRENT_EXPORT;

std::string string_with_escape_codes(const std::string& str) LIBTORRENT_EXPORT;

std::string sanitize_string(const std::string& str) LIBTORRENT_EXPORT;
std::string sanitize_string_with_escape_codes(const std::string& str) LIBTORRENT_EXPORT;
std::string sanitize_string_with_tags(const std::string& str) LIBTORRENT_EXPORT;

} // namespace torrent::utils

#endif // LIBTORRENT_TORRENT_UTILS_STRING_MANIP_H
