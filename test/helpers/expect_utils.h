#ifndef LIBTORRENT_HELPER_EXPECT_UTILS_H
#define LIBTORRENT_HELPER_EXPECT_UTILS_H

#include "helpers/mock_function.h"

#include <torrent/utils/random.h>

inline void
expect_random_uniform_uint16(uint16_t result, uint16_t first, uint16_t last) {
  mock_expect(&torrent::random_uniform_uint16, result, first, last);
}

#endif
