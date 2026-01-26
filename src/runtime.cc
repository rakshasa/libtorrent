#include "config.h"

#include "runtime.h"

#include "torrent/runtime/network_manager.h"
#include "torrent/runtime/socket_manager.h"

namespace torrent {

Runtime* g_runtime = nullptr;

// TODO: Move runtime namespace stuff here.

Runtime::Runtime()
  : m_network_manager(new runtime::NetworkManager),
    m_socket_manager(new runtime::SocketManager) {
}

Runtime::~Runtime() {
  // Order matters for destruction.
  m_network_manager.reset();
  m_socket_manager.reset();
}

} // namespace torrent
