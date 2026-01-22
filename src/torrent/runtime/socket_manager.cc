#include "config.h"

#include "torrent/net/socket_manager.h"

// #include <cerrno>

namespace torrent::net {

SocketManager::SocketManager() {
  // TODO: Set load factor a bit higher than default to account for peak usage during startup / etc.
}

SocketManager::~SocketManager() {
}

int
SocketManager::open_socket(std::function<int ()> socket_func) {
  auto guard = lock_guard();

  int fd = socket_func();

  // TOOD: Add logging.
  if (fd == -1) {
    return -1;
  }

  m_socket_map.emplace(fd, nullptr);

  return fd;
}

bool



} // namespace torrent::net
