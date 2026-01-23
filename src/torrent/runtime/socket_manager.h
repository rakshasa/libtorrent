#ifndef LIBTORRENT_TORRENT_RUNTIME_SOCKET_MANAGER_H
#define LIBTORRENT_TORRENT_RUNTIME_SOCKET_MANAGER_H

#include <mutex>
#include <unordered_map>
#include <torrent/common.h>

namespace torrent::runtime {

struct SocketInfo {
  int                 fd{-1};
  utils::Thread*      thread{nullptr};
};

class SocketManager {
public:
  SocketManager();
  ~SocketManager();

  // TODO: Add open_socket version that automatically closes the new fd if there's a conflict.

  int                 open_socket(std::function<int ()> socket_func);
  bool                close_socket(int fd, std::function<int (int)> close_func);

  // TODO: Add FILE* open.

protected:

  auto                lock_guard() { return std::lock_guard(m_mutex); }

private:
  using socket_map_type = std::unordered_map<int, SocketInfo>;

  std::mutex          m_mutex;

  socket_map_type     m_socket_map;
};

} // namespace torrent::runtime

#endif // LIBTORRENT_TORRENT_RUNTIME_SOCKET_MANAGER_H
