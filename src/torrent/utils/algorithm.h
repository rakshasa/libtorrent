#ifndef LIBTORRENT_TORRENT_UTILS_ALGORITHM_H
#define LIBTORRENT_TORRENT_UTILS_ALGORITHM_H

#include <iterator>
#include <tuple>

namespace torrent::utils {

template <typename T, typename Container>
std::tuple<T, typename Container::iterator, bool>
find_unused_key(Container& container, T start_key, T min_key, T max_key) {
  auto key = start_key;
  auto itr = container.lower_bound(key);

  while (true) {
    if (itr == container.end() || itr->first != key)
      return {key, itr, true};

    if (key == max_key) {
      key = min_key;
      itr = container.lower_bound(key);
    } else {
      key++;
      itr = std::next(itr);
    }

    if (key == start_key)
      return {{}, {}, false};
  }
}

} // namespace torrent::utils

#endif
