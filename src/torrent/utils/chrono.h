#ifndef TORRENT_UTILS_CHRONO_H
#define TORRENT_UTILS_CHRONO_H

#include <chrono>

using namespace std::chrono_literals;

namespace torrent::utils {

inline std::chrono::microseconds
time_since_epoch() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
}

// std::chrono::microseconds
// ceil_seconds(std::chrono::microseconds t) {
//   return std::chrono::duration_cast<std::chrono::microseconds>(t.ceil<std::chrono::seconds>());
// }

}

#endif // TORRENT_UTILS_CHRONO_H
