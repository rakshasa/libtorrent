#ifndef LIBTORRENT_RUNTIME_H
#define LIBTORRENT_RUNTIME_H

#include "torrent/runtime/runtime.h"

namespace torrent {

class LIBTORRENT_EXPORT Runtime {
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

  auto*               network_manager();
  auto*               socket_manager();
  auto*               proxy_manager();

private:
  Runtime();
  ~Runtime();

  std::unique_ptr<runtime::NetworkConfig>  m_network_config;

  std::unique_ptr<runtime::NetworkManager> m_network_manager;
  std::unique_ptr<runtime::SocketManager>  m_socket_manager;
  std::unique_ptr<runtime::ProxyManager>   m_proxy_manager;

  // TODO: Check if we got this elsewhere?
  align_cacheline std::atomic<bool>        m_initialized{};
  std::atomic<bool>                        m_shutdown_called{};
  std::atomic<bool>                        m_quick_shutdown_called{};
};

inline bool Runtime::is_initialized()           { return m_initialized.load(std::memory_order_acquire); }
inline bool Runtime::is_shutdown_called()       { return m_shutdown_called.load(std::memory_order_acquire); }
inline bool Runtime::is_quick_shutdown_called() { return m_quick_shutdown_called.load(std::memory_order_acquire); }

inline auto* Runtime::network_config()          { return m_network_config.get(); }

inline auto* Runtime::network_manager()         { return m_network_manager.get(); }
inline auto* Runtime::socket_manager()          { return m_socket_manager.get(); }
inline auto* Runtime::proxy_manager()           { return m_proxy_manager.get(); }

} // namespace torrent

#endif // LIBTORRENT_RUNTIME_H
