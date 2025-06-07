#ifndef TORRENT_UTILS_CHRONO_H
#define TORRENT_UTILS_CHRONO_H

#include <chrono>

using namespace std::chrono_literals;

namespace torrent::utils {

std::chrono::microseconds ceil_seconds(std::chrono::microseconds t);
std::chrono::seconds      cast_seconds(std::chrono::microseconds t);
std::chrono::microseconds time_since_epoch();

// Implementation:

inline std::chrono::seconds
cast_seconds(std::chrono::microseconds t) {
  return std::chrono::duration_cast<std::chrono::seconds>(t);
}

inline std::chrono::microseconds
ceil_seconds(std::chrono::microseconds t) {
  // C++17
  // return std::chrono::duration_cast<std::chrono::microseconds>(t.ceil<std::chrono::seconds>());

  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(t + 1s - 1us);

  return std::chrono::duration_cast<std::chrono::microseconds>(seconds);
}

inline std::chrono::seconds
ceil_cast_seconds(std::chrono::microseconds t) {
  return std::chrono::duration_cast<std::chrono::seconds>(t + 1s - 1us);
}

inline std::chrono::microseconds
time_since_epoch() {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
}

} // namespace torrent::utils

#endif // TORRENT_UTILS_CHRONO_H
