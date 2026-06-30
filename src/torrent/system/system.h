#ifndef LIBTORRENT_TORRENT_SYSTEM_SYSTEM_H
#define LIBTORRENT_TORRENT_SYSTEM_SYSTEM_H

#include <atomic>
#include <memory>
#include <string>
#include <torrent/common.h>

namespace RTORRENT_EXPORT torrent {

namespace system {

const char*         errno_enum(int status);
std::string         errno_enum_str(int status);

const char*         gai_enum_error(int status);
std::string         gai_enum_error_str(int status);

} // namespace torrent::system

} // namespace torrent

#endif
