#include "config.h"

#include "random.h"

#include "torrent/exceptions.h"

namespace torrent {

// TODO: Replace with std and thread_local generator.

template <typename T>
T
random_uniform_template(T min, T max) {
  if (min > max)
    throw internal_error("random_uniform: min > max");

  if (min == max)
    return min;

  std::random_device rd;
  std::mt19937 mt(rd());

  return min + std::uniform_int_distribution<T>(min, max)(mt) % (max - min + 1);
}  

uint16_t random_uniform_uint16(uint16_t min, uint16_t max) { return random_uniform_template<uint16_t>(min, max); }
uint32_t random_uniform_uint32(uint32_t min, uint32_t max) { return random_uniform_template<uint32_t>(min, max); }

}
