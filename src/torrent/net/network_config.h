#ifndef LIBTORRENT_TORRENT_NET_NETWORK_CONFIG_H
#define LIBTORRENT_TORRENT_NET_NETWORK_CONFIG_H

#include <mutex>
#include <torrent/net/types.h>

namespace torrent::net {

class LIBTORRENT_EXPORT NetworkConfig {
public:
  NetworkConfig();

  // TODO: Calls using these should be thread safe, and a connection manager should be responsible
  // for binding, etc, fds.
  //
  // When we change the * addresses, we should lock the connection manager until both pre- and
  // post-change handling is done. This would include closing and reopening listening sockets,
  // resetting connections, restarting dht, etc.

  // TODO: Http bind address should be moved here.
  // TODO: Move helper functions in rtorrent manager here.

  // Http stack has an independent bind address setting.
  //
  // We should set the main http bind address in set_bind_address, and have special handling if it
  // is overridden by the user.

  c_sa_shared_ptr     bind_address() const;
  std::string         bind_address_str() const;
  void                set_bind_address(const sockaddr* sa);

  c_sa_shared_ptr     local_address() const;
  std::string         local_address_str() const;
  void                set_local_address(const sockaddr* sa);

  c_sa_shared_ptr     proxy_address() const;
  std::string         proxy_address_str() const;
  void                set_proxy_address(const sockaddr* sa);

  // Port number should not be cleared as it is used for tracker announces.
  uint16_t            listen_port() const;
  uint16_t            listen_port_or_throw() const;
  int                 listen_backlog() const;

  uint16_t            override_dht_port() const;
  void                set_override_dht_port(uint16_t port);

protected:
  friend class torrent::ConnectionManager;

  void                lock() const                    { m_mutex.lock(); }
  auto                lock_guard() const              { return std::lock_guard(m_mutex); }
  void                unlock() const                  { m_mutex.unlock(); }
  auto&               mutex() const                   { return m_mutex; }

  // TODO: Use different function for client updating port.
  void                set_listen_port(uint16_t port);
  void                set_listen_backlog(int backlog);

private:
  mutable std::mutex  m_mutex;

  c_sa_shared_ptr     m_bind_address;
  c_sa_shared_ptr     m_local_address;
  c_sa_shared_ptr     m_proxy_address;

  uint16_t            m_listen_port{0};
  int                 m_listen_backlog{SOMAXCONN};
  uint16_t            m_override_dht_port{0};
};

} // namespace torrent::net

#endif
