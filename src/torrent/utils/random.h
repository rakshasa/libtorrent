#ifndef LIBTORRENT_UTILS_RANDOM_H
#define LIBTORRENT_UTILS_RANDOM_H

#include <limits>
#include <random>
#include <torrent/common.h>

namespace torrent {

[[gnu::weak]] uint16_t random_uniform_uint16(uint16_t min = std::numeric_limits<uint16_t>::min(), uint16_t max = std::numeric_limits<uint16_t>::max()) LIBTORRENT_EXPORT;
[[gnu::weak]] uint32_t random_uniform_uint32(uint32_t min = std::numeric_limits<uint32_t>::min(), uint32_t max = std::numeric_limits<uint32_t>::max()) LIBTORRENT_EXPORT;

}

#endif
