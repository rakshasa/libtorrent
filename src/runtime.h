#ifndef LIBTORRENT_RUNTIME_H
#define LIBTORRENT_RUNTIME_H

#include "config.h"

#include <memory>

#include "torrent/common.h"

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

  bool                webtorrent_enabled() const;
  void                set_webtorrent_enabled(bool enabled);

  auto*               network_config();

  auto*               network_manager();
  auto*               socket_manager();

private:
  Runtime();
  ~Runtime();

  std::unique_ptr<runtime::NetworkConfig>  m_network_config;

  std::unique_ptr<runtime::NetworkManager> m_network_manager;
  std::unique_ptr<runtime::SocketManager>  m_socket_manager;

  // TODO: Check if we got this elsewhere?
  align_cacheline std::atomic<bool>        m_initialized{};
  std::atomic<bool>                        m_shutdown_called{};
  std::atomic<bool>                        m_quick_shutdown_called{};
#ifdef USE_WEBTORRENT
  std::atomic<bool>                        m_webtorrent_enabled{true};
#else
  std::atomic<bool>                        m_webtorrent_enabled{false};
#endif
};

inline bool Runtime::is_initialized()           { return m_initialized.load(std::memory_order_acquire); }
inline bool Runtime::is_shutdown_called()       { return m_shutdown_called.load(std::memory_order_acquire); }
inline bool Runtime::is_quick_shutdown_called() { return m_quick_shutdown_called.load(std::memory_order_acquire); }
inline bool Runtime::webtorrent_enabled() const { return m_webtorrent_enabled.load(std::memory_order_acquire); }
inline void Runtime::set_webtorrent_enabled(bool enabled) {
#ifdef USE_WEBTORRENT
  m_webtorrent_enabled.store(enabled, std::memory_order_release);
#else
  m_webtorrent_enabled.store(false, std::memory_order_release);
#endif
}

inline auto* Runtime::network_config()          { return m_network_config.get(); }

inline auto* Runtime::network_manager()         { return m_network_manager.get(); }
inline auto* Runtime::socket_manager()          { return m_socket_manager.get(); }

} // namespace torrent

#endif // LIBTORRENT_RUNTIME_H
