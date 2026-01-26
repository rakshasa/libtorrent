#ifndef LIBTORRENT_RUNTIME_H
#define LIBTORRENT_RUNTIME_H

#include <memory>

#include "torrent/common.h"

namespace torrent {

class Manager;

class Runtime {
public:
  Runtime();
  ~Runtime();

protected:
  friend Manager;

  auto*               network_manager()    { return m_network_manager.get(); }
  auto*               socket_manager()     { return m_socket_manager.get(); }

private:
  std::unique_ptr<runtime::NetworkManager> m_network_manager;
  std::unique_ptr<runtime::SocketManager>  m_socket_manager;
};

extern Runtime* g_runtime;

} // namespace torrent

#endif // LIBTORRENT_RUNTIME_H
