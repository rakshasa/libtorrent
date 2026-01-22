#include "config.h"

#include "torrent/runtime/socket_manager.h"

#include <cassert>

#include "torrent/exceptions.h"
#include "torrent/utils/log.h"
#include "torrent/utils/thread.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_NET_SOCKET, "socket_manager: " log_fmt, __VA_ARGS__);

namespace torrent::net {

SocketManager::SocketManager() {
  // TODO: Set load factor a bit higher than default to account for peak usage during startup / etc.
}

SocketManager::~SocketManager() {
  assert(m_socket_map.empty() && "SocketManager::~SocketManager(): socket map not empty on destruction.");
}

int
SocketManager::open_socket(std::function<int ()> socket_func) {
  auto guard = lock_guard();

  int fd = socket_func();

  if (fd == -1) {
    LT_LOG("open_socket(%i) : %s : failed to open socket", fd, this_thread::thread()->name());
    return -1;
  }

  auto itr = m_socket_map.find(fd);

  // TODO: This isn't really an error, rather we assume the fd is reused.
  if (itr != m_socket_map.end()) {
    LT_LOG("open_socket(%i) : %s : trying to open already existing socket : thread:%s",
           fd, this_thread::thread()->name(), itr->second.thread->name());

    throw internal_error("SocketManager::open_socket(): trying to open already existing socket fd: " + std::to_string(fd));
  }

  LT_LOG("open_socket(%i) : %s : opened socket", fd, this_thread::thread()->name());

  m_socket_map.emplace(fd, SocketInfo{fd, this_thread::thread()});

  return fd;
}

bool
SocketManager::close_socket(int fd, std::function<int (int)> close_func) {
  auto guard = lock_guard();

  auto itr = m_socket_map.find(fd);

  if (itr == m_socket_map.end())
    throw internal_error("SocketManager::close_socket(): trying to close unknown socket fd: " + std::to_string(fd));

  if (itr->second.thread != this_thread::thread())
    throw internal_error("SocketManager::close_socket(" + std::to_string(fd) + "): trying to close socket from wrong thread : " +
                         itr->second.thread->name() + " != " + this_thread::thread()->name());

  errno = 0;

  int result = close_func(fd);

  if (result == -1) {
    LT_LOG("close_socket(%i) : %s : failed to close socket : %s", fd, this_thread::thread()->name(), std::strerror(errno));
    return false;
  }

  LT_LOG("close_socket(%i) : %s : closed socket", fd, this_thread::thread()->name());

  m_socket_map.erase(itr);

  return true;
}

} // namespace torrent::net
