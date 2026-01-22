#ifndef LIBTORRENT_TORRENT_NET_SOCKET_MANAGER_H
#define LIBTORRENT_TORRENT_NET_SOCKET_MANAGER_H

#include <functional>
#include <mutex>
#include <unordered_map>
#include <torrent/common.h>

namespace torrent::net {

struct SocketInfo {
  int                 fd{-1};

  // thread, event, type, etc.
  utils::Thread*      thread{nullptr};
};

class SocketManager {
public:
  SocketManager();
  ~SocketManager();

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

} // namespace torrent::net

#endif // LIBTORRENT_TORRENT_NET_SOCKET_MANAGER_H
