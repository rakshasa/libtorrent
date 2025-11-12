#ifndef LIBTORRENT_TORRENT_NET_NETWORK_MANAGER_H
#define LIBTORRENT_TORRENT_NET_NETWORK_MANAGER_H

#include <mutex>
#include <torrent/net/types.h>

namespace torrent {

class Manager;

} // namespace torrent

namespace torrent::net {

class LIBTORRENT_EXPORT NetworkManager {
public:
  NetworkManager();
  ~NetworkManager();

  bool                is_listening() const;
  bool                is_dht_valid() const;
  bool                is_dht_active() const;
  bool                is_dht_active_and_receiving_requests() const;

  void                cleanup();

  // TODO: Change to have network_config hold the port range / random info.

  bool                listen_open(uint16_t first, uint16_t last);
  void                listen_close();

  // Port number remains set after listen_close.
  uint16_t            listen_port() const;
  uint16_t            listen_port_or_throw() const;

  // TODO: Only allowed to be called from main thread (tracker thread when moved).
  // TODO: Move DHT on/off/auto handlig here.

  auto*               dht_controller();
  uint16_t            dht_port();

  void                dht_add_bootstrap_node(std::string host, int port);
  void                dht_add_peer_node(const sockaddr* sa, int port);

protected:
  friend class torrent::Manager;
  friend class torrent::net::NetworkConfig;

  void                lock() const                    { m_mutex.lock(); }
  auto                lock_guard() const              { return std::lock_guard(m_mutex); }
  void                unlock() const                  { m_mutex.unlock(); }
  auto&               mutex() const                   { return m_mutex; }

  bool                is_listening_unsafe() const;

  auto                listen_inet_unsafe()            { return m_listen_inet.get(); }
  auto                listen_inet6_unsafe()           { return m_listen_inet6.get(); }

  void                restart_listen();

private:
  bool                listen_open_unsafe(uint16_t first, uint16_t last);
  void                listen_close_unsafe();

  void                perform_restart_listen();

  mutable std::mutex      m_mutex;

  std::unique_ptr<Listen> m_listen_inet;
  std::unique_ptr<Listen> m_listen_inet6;
  uint16_t                m_listen_port{0};
  bool                    m_listen_restarting{};

  std::unique_ptr<tracker::DhtController> m_dht_controller;
};

// We don't need locking for objects we initialize/destruct in the ctor/dtor.

inline auto* NetworkManager::dht_controller() { return m_dht_controller.get(); }

} // namespace torrent::net

#endif // LIBTORRENT_TORRENT_NET_NETWORK_MANAGER_H
