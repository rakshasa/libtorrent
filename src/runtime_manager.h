#ifndef LIBTORRENT_RUNTIME_MANAGER_H
#define LIBTORRENT_RUNTIME_MANAGER_H

#include "torrent/runtime/runtime.h"

namespace torrent {

class LIBTORRENT_EXPORT RuntimeManager {
public:
  static void         initialize();

  static void         shutdown();
  static void         quick_shutdown();

  static void         cleanup();
  static void         destroy();

  bool                is_initialized();
  bool                is_shutdown_called();
  bool                is_quick_shutdown_called();

  auto*               network_config();

  auto*               memory_manager();
  auto*               network_manager();
  auto*               socket_manager();
  auto*               proxy_manager();

private:
  RuntimeManager();
  ~RuntimeManager();

  runtime::NetworkConfig*  m_network_config;

  runtime::MemoryManager*  m_memory_manager;
  runtime::NetworkManager* m_network_manager;
  runtime::SocketManager*  m_socket_manager;
  runtime::ProxyManager*   m_proxy_manager;

  align_cacheline

  std::atomic<bool>        m_initialized{};
  std::atomic<bool>        m_shutdown_called{};
  std::atomic<bool>        m_quick_shutdown_called{};
};

inline bool RuntimeManager::is_initialized()           { return m_initialized.load(std::memory_order_acquire); }
inline bool RuntimeManager::is_shutdown_called()       { return m_shutdown_called.load(std::memory_order_acquire); }
inline bool RuntimeManager::is_quick_shutdown_called() { return m_quick_shutdown_called.load(std::memory_order_acquire); }

inline auto* RuntimeManager::network_config()          { return m_network_config; }

inline auto* RuntimeManager::memory_manager()          { return m_memory_manager; }
inline auto* RuntimeManager::network_manager()         { return m_network_manager; }
inline auto* RuntimeManager::socket_manager()          { return m_socket_manager; }
inline auto* RuntimeManager::proxy_manager()           { return m_proxy_manager; }

} // namespace torrent

#endif
