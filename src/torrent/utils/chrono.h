#ifndef TORRENT_UTILS_CHRONO_H
#define TORRENT_UTILS_CHRONO_H

#include <chrono>

using namespace std::chrono_literals;

namespace torrent::utils {

// std::chrono::microseconds cached_time_with_timer(std::chrono::microseconds timeout);
std::chrono::microseconds time_since_epoch();
std::chrono::microseconds ceil_seconds(std::chrono::microseconds t);

// Implementation:

inline std::chrono::microseconds
time_since_epoch() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
}

inline std::chrono::microseconds
ceil_seconds(std::chrono::microseconds t) {
  // C++17
  // return std::chrono::duration_cast<std::chrono::microseconds>(t.ceil<std::chrono::seconds>());

  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(t + 1s - 1us);

  return std::chrono::duration_cast<std::chrono::microseconds>(seconds);
}

}

#endif // TORRENT_UTILS_CHRONO_H
