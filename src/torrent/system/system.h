#ifndef LIBTORRENT_TORRENT_SYSTEM_SYSTEM_H
#define LIBTORRENT_TORRENT_SYSTEM_SYSTEM_H

#include <atomic>
#include <memory>
#include <string>
#include <torrent/common.h>

namespace torrent::system {

const char*         errno_enum(int status) LIBTORRENT_EXPORT;
std::string         errno_enum_str(int status) LIBTORRENT_EXPORT;

} // namespace torrent::system

#endif
