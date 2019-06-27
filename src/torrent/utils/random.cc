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

  if (min == std::numeric_limits<T>::min() && max == std::numeric_limits<T>::max())
    return (T)rand();

  return min + (T)rand() % (max - min + 1);
}  

uint32_t
random_uniform_uint32(uint32_t min, uint32_t max) {
  return random_uniform_template<uint32_t>(min, max);
}

}
