#ifndef LIBTORRENT_TORRENT_UTILS_RANDOM_H
#define LIBTORRENT_TORRENT_UTILS_RANDOM_H

#include <cinttypes>
#include <limits>
#include <random>

namespace torrent {

[[gnu::weak]] [[gnu::visibility("default")]] uint16_t random_uniform_uint16(uint16_t min = std::numeric_limits<uint16_t>::min(), uint16_t max = std::numeric_limits<uint16_t>::max());
[[gnu::weak]] [[gnu::visibility("default")]] uint32_t random_uniform_uint32(uint32_t min = std::numeric_limits<uint32_t>::min(), uint32_t max = std::numeric_limits<uint32_t>::max());

}

#endif
