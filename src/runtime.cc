#include "config.h"

#include "runtime.h"

#include "torrent/runtime/network_manager.h"
#include "torrent/runtime/socket_manager.h"

namespace torrent {

Runtime* g_runtime{};

namespace runtime {

NetworkManager*  network_manager()                                    { return g_runtime->network_manager(); }
SocketManager*   socket_manager()                                     { return g_runtime->socket_manager(); }

void             dht_add_peer_node(const sockaddr* sa, uint16_t port) { g_runtime->network_manager()->dht_add_peer_node(sa, port); }
uint16_t         listen_port()                                        { return g_runtime->network_manager()->listen_port(); }

} // namespace runtime

Runtime::Runtime(system::Thread* main_thread)
  : m_main_thread(main_thread),

    m_network_manager(new runtime::NetworkManager(main_thread)),
    m_socket_manager(new runtime::SocketManager) {
}

Runtime::~Runtime() = default;

void
Runtime::initialize(system::Thread* main_thread) {
  g_runtime = new Runtime(main_thread);
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

  delete g_runtime;
  g_runtime = nullptr;
}

} // namespace torrent
