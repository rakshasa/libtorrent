#include "config.h"

#include "runtime.h"

#include "torrent/runtime/network_config.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/runtime/socket_manager.h"

namespace torrent {

Runtime* g_runtime{};

namespace runtime {

NetworkConfig*   network_config()                                     { return g_runtime->network_config(); }

NetworkManager*  network_manager()                                    { return g_runtime->network_manager(); }
SocketManager*   socket_manager()                                     { return g_runtime->socket_manager(); }

const char*      version()                                            { return VERSION; }

uint16_t         listen_port()                                        { return g_runtime->network_manager()->listen_port(); }

void             dht_add_peer_node(const sockaddr* sa, uint16_t port) { g_runtime->network_manager()->dht_add_peer_node(sa, port); }

} // namespace runtime

Runtime::Runtime()
  : m_network_config(new runtime::NetworkConfig),
    m_network_manager(new runtime::NetworkManager()),
    m_socket_manager(new runtime::SocketManager) {
}

Runtime::~Runtime() = default;

void
Runtime::initialize() {
  g_runtime = new Runtime();
}

void
Runtime::cleanup() {
  runtime::network_manager()->listen_close();
}

void
Runtime::destroy() {
  // Order matters for destruction.
  g_runtime->m_network_manager.reset();
  g_runtime->m_socket_manager.reset();

  g_runtime->m_network_config.reset();

  delete g_runtime;
  g_runtime = nullptr;
}

} // namespace torrent
