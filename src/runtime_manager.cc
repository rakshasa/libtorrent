#include "config.h"

#include "runtime_manager.h"

#include "torrent/exceptions.h"
#include "torrent/runtime/client_config.h"
#include "torrent/runtime/memory_manager.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/runtime/runtime.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/runtime/proxy_manager.h"

namespace torrent {

RuntimeManager* g_runtime{};

namespace runtime {

bool             is_initialized()         { return g_runtime->is_initialized(); }
bool             is_network_initialized() { return g_runtime->is_network_initialized(); }
bool             is_shutting_down()       { return g_runtime->is_shutdown_called(); }
bool             is_quick_shutting_down() { return g_runtime->is_quick_shutdown_called(); }

void             initialize_network()     { g_runtime->initialize_network(); }
void             shutdown()               { g_runtime->shutdown(); }
void             quick_shutdown()         { g_runtime->quick_shutdown(); }

ClientConfig*    client_config()          { return g_runtime->client_config(); }
NetworkConfig*   network_config()         { return g_runtime->network_config(); }

MemoryManager*   memory_manager()         { return g_runtime->memory_manager(); }
NetworkManager*  network_manager()        { return g_runtime->network_manager(); }
SocketManager*   socket_manager()         { return g_runtime->socket_manager(); }
ProxyManager*    proxy_manager()          { return g_runtime->proxy_manager(); }

} // namespace runtime

RuntimeManager::RuntimeManager()
  : m_client_config(new runtime::ClientConfig),
    m_network_config(new runtime::NetworkConfig),

    m_memory_manager(new runtime::MemoryManager),
    m_network_manager(new runtime::NetworkManager),
    m_socket_manager(new runtime::SocketManager),
    m_proxy_manager(new runtime::ProxyManager) {
}

RuntimeManager::~RuntimeManager() = default;

void
RuntimeManager::initialize() {
  g_runtime = new RuntimeManager();
  g_runtime->m_initialized = true;
}

void
RuntimeManager::initialize_network() {
  g_runtime->m_network_initialized = true;

  auto listen_port = g_runtime->m_network_manager->listen_port();

  if (listen_port != 0) {
    g_runtime->m_network_manager->listen_open(listen_port, listen_port);
    return;
  }

  auto port_range = g_runtime->m_client_config->listen_port_range();

  if (g_runtime->m_client_config->listen_port_random()) {
    uint16_t boundary = port_range.first + (random() % (port_range.second - port_range.first + 1));

    if (g_runtime->m_network_manager->listen_open(boundary, port_range.second) ||
        g_runtime->m_network_manager->listen_open(port_range.first, boundary))
      return;

  } else {
    if (g_runtime->m_network_manager->listen_open(port_range.first, port_range.second))
      return;
  }

  throw input_error("Could not open/bind port for listening: " + std::string(std::strerror(errno)));
}

// TODO: When changing listen port right before shutdown, it takes 10s for udp_routers to
// close. Likely rtorrent thinks downloads are not done, or something. (even if all are closed)

void
RuntimeManager::shutdown() {
  g_runtime->m_shutdown_called = true;
}

void
RuntimeManager::quick_shutdown() {
  g_runtime->m_shutdown_called       = true;
  g_runtime->m_quick_shutdown_called = true;

  // TODO: This should e.g. timeout all udp, http, dns, etc requests.
}

void
RuntimeManager::cleanup() {
  g_runtime->m_network_manager->listen_close();
}

void
RuntimeManager::destroy() {
  // Order matters for destruction.
  //
  // Doing manual delete so we can keep the ctor/dtors non-public.

  delete g_runtime->m_proxy_manager;
  g_runtime->m_proxy_manager = nullptr;

  delete g_runtime->m_socket_manager;
  g_runtime->m_socket_manager = nullptr;

  delete g_runtime->m_network_manager;
  g_runtime->m_network_manager = nullptr;

  delete g_runtime->m_memory_manager;
  g_runtime->m_memory_manager = nullptr;

  delete g_runtime->m_client_config;
  g_runtime->m_client_config = nullptr;

  delete g_runtime->m_network_config;
  g_runtime->m_network_config = nullptr;

  delete g_runtime;
  g_runtime = nullptr;
}

} // namespace torrent
