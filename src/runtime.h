#ifndef LIBTORRENT_RUNTIME_H
#define LIBTORRENT_RUNTIME_H

#include <memory>

#include "torrent/common.h"

namespace torrent {

class LIBTORRENT_EXPORT Runtime {
public:
  static void         initialize(system::Thread* main_thread);
  static void         cleanup();
  static void         destroy();

  auto*               network_manager()    { return m_network_manager.get(); }
  auto*               socket_manager()     { return m_socket_manager.get(); }

private:
  Runtime(system::Thread* main_thread);
  ~Runtime();

  system::Thread*     m_main_thread;

  std::unique_ptr<runtime::NetworkManager> m_network_manager;
  std::unique_ptr<runtime::SocketManager>  m_socket_manager;
};

} // namespace torrent

#endif // LIBTORRENT_RUNTIME_H
