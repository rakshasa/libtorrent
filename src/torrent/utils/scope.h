#ifndef LIBTORRENT_TORRENT_UTILS_SCOPE_H
#define LIBTORRENT_TORRENT_UTILS_SCOPE_H

#include <functional>

namespace torrent::utils {

class scope_exit {
public:
  scope_exit(std::function<void()> fn);
  ~scope_exit();

  void                release();

private:
  std::function<void()> m_fn;
};

inline scope_exit::scope_exit(std::function<void()> fn) : m_fn(fn) {}
inline scope_exit::~scope_exit() { m_fn(); }

inline void scope_exit::release() { m_fn = []() {}; }

} // namespace torrent::utils

#endif
