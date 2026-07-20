#ifndef LIBTORRENT_TORRENT_SYSTEM_TYPES_H
#define LIBTORRENT_TORRENT_SYSTEM_TYPES_H

#include <torrent/common.h>

namespace torrent::system {

const char*         errno_enum(int status) LIBTORRENT_EXPORT;
inline std::string  errno_enum_str(int status);

const char*         sa_family_enum(int family) LIBTORRENT_EXPORT;
inline std::string  sa_family_enum_str(int family);

const char*         gai_enum_error(int status) LIBTORRENT_EXPORT;
inline std::string  gai_enum_error_str(int status);


//
// Implemention:
//

std::string errno_enum_str(int status)     { return errno_enum(status); }
std::string sa_family_enum_str(int family) { return sa_family_enum(family); }
std::string gai_enum_error_str(int status) { return gai_enum_error(status); }

} // namespace torrent::system

#endif
