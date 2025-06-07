#include "config.h"

#include "random.h"

#include "torrent/exceptions.h"

namespace torrent {

template <typename T>
std::enable_if_t<std::is_integral_v<T>, T>
static random_uniform_template(T min, T max) {
  if (min > max)
    throw internal_error("random_uniform: min > max");

  if (min == max)
    return min;

  static thread_local std::mt19937 mt(std::random_device{}());
  return std::uniform_int_distribution<T>(min, max)(mt);
}  

uint16_t random_uniform_uint16(uint16_t min, uint16_t max) { return random_uniform_template<uint16_t>(min, max); }
uint32_t random_uniform_uint32(uint32_t min, uint32_t max) { return random_uniform_template<uint32_t>(min, max); }

} // namespace torrent
