#ifndef LIBTORRENT_TORRENT_NET_NETWORK_CONFIG_H
#define LIBTORRENT_TORRENT_NET_NETWORK_CONFIG_H

#include <mutex>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <torrent/net/types.h>

namespace torrent::net {

// TODO: Add a connection setup class that holds basic information about a connection, like previous
// failed attempts, what inet family/bind were used, etc.
//
// ConnectionInfo ?
// ConnectionOptions ?
// ConnectionSettings ?
// ConnectionContext ?
// ConnectionState ?

class LIBTORRENT_EXPORT NetworkConfig {
public:
  static constexpr int iptos_default     = 0;
  static constexpr int iptos_lowdelay    = IPTOS_LOWDELAY;
  static constexpr int iptos_throughput  = IPTOS_THROUGHPUT;
  static constexpr int iptos_reliability = IPTOS_RELIABILITY;

  static constexpr uint32_t encryption_none             = 0;
  static constexpr uint32_t encryption_allow_incoming   = 0x1;
  static constexpr uint32_t encryption_try_outgoing     = 0x2;
  static constexpr uint32_t encryption_require          = 0x3;
  static constexpr uint32_t encryption_require_RC4      = 0x4;
  static constexpr uint32_t encryption_enable_retry     = 0x8;
  static constexpr uint32_t encryption_prefer_plaintext = 0x10;
  // Internal to libtorrent.
  static constexpr uint32_t encryption_use_proxy        = 0x20;
  static constexpr uint32_t encryption_retrying         = 0x40;

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

  bool                is_block_ipv4() const;
  bool                is_block_ipv6() const;
  bool                is_block_ipv4in6() const;
  bool                is_block_outgoing() const;
  bool                is_prefer_ipv6() const;

  void                set_block_ipv4(bool v);
  void                set_block_ipv6(bool v);
  void                set_block_ipv4in6(bool v);
  void                set_block_outgoing(bool v);
  void                set_prefer_ipv6(bool v);

  int                 priority() const;
  void                set_priority(int p);

  // Use bind_address_best_match for display purposes only, or if you manually check blocking.
  //
  // If bind_address_or_null returns null, then available protocols are blocked. This means if the
  // user binds to e.g. inet6 and blocks ipv6, then no connections are possible.
  //
  // To bind one and keep the other protocol unbound, set the other address to either 0.0.0.0 or ::.

  c_sa_shared_ptr     bind_address_best_match() const;
  std::string         bind_address_best_match_str() const;
  c_sa_shared_ptr     bind_address_or_unspec_and_null() const;
  c_sa_shared_ptr     bind_address_or_any_and_null() const;
  c_sa_shared_ptr     bind_address_for_connect(int family) const;

  c_sa_shared_ptr     bind_inet_address() const;
  std::string         bind_inet_address_str() const;
  c_sa_shared_ptr     bind_inet6_address() const;
  std::string         bind_inet6_address_str() const;

  c_sa_shared_ptr     local_address_best_match() const;
  std::string         local_address_best_match_str() const;
  c_sa_shared_ptr     local_address_or_unspec_and_null() const;
  c_sa_shared_ptr     local_address_or_any_and_null() const;

  c_sa_shared_ptr     local_inet_address() const;
  c_sa_shared_ptr     local_inet_address_or_null() const;
  std::string         local_inet_address_str() const;
  c_sa_shared_ptr     local_inet6_address() const;
  c_sa_shared_ptr     local_inet6_address_or_null() const;
  std::string         local_inet6_address_str() const;

  c_sa_shared_ptr     proxy_address() const;
  std::string         proxy_address_str() const;

  // When set_bind_inet*_address is used, http stack bind address needs to be manually set.
  //
  // TODO: Change http stack to use NetworkConfig
  void                set_bind_address(const sockaddr* sa);
  void                set_bind_address_str(const std::string& addr);
  void                set_bind_inet_address(const sockaddr* sa);
  void                set_bind_inet_address_str(const std::string& addr);
  void                set_bind_inet6_address(const sockaddr* sa);
  void                set_bind_inet6_address_str(const std::string& addr);
  void                set_local_address(const sockaddr* sa);
  void                set_local_address_str(const std::string& addr);
  void                set_local_inet_address(const sockaddr* sa);
  void                set_local_inet_address_str(const std::string& addr);
  void                set_local_inet6_address(const sockaddr* sa);
  void                set_local_inet6_address_str(const std::string& addr);
  void                set_proxy_address(const sockaddr* sa);

  // Port number should not be cleared as it is used for tracker announces.
  //
  // TODO: Move to NM.
  uint16_t            listen_port() const;
  uint16_t            listen_port_or_throw() const;

  uint16_t            override_dht_port() const;
  void                set_override_dht_port(uint16_t port);

  uint32_t            encryption_options() const;
  void                set_encryption_options(uint32_t opts);

  uint32_t            send_buffer_size() const;
  uint32_t            receive_buffer_size() const;

  void                set_send_buffer_size(uint32_t s);
  void                set_receive_buffer_size(uint32_t s);

protected:
  friend class torrent::ConnectionManager;
  friend class torrent::net::NetworkManager;

  void                lock() const                    { m_mutex.lock(); }
  auto                lock_guard() const              { return std::lock_guard(m_mutex); }
  void                unlock() const                  { m_mutex.unlock(); }
  auto&               mutex() const                   { return m_mutex; }

  // TODO: Use different function for client updating port.
  void                set_listen_port(uint16_t port);
  void                set_listen_port_unsafe(uint16_t port);

private:
  c_sa_shared_ptr     generic_address_best_match(const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const;
  std::string         generic_address_best_match_str(const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const;
  c_sa_shared_ptr     generic_address_or_unspec_and_null(const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const;
  c_sa_shared_ptr     generic_address_or_any_and_null(const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const;
  c_sa_shared_ptr     generic_address_for_connect(int family, const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const;
  c_sa_shared_ptr     generic_address_for_family(int family, const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const;

  void                set_generic_address(const char* category, c_sa_shared_ptr& inet_address, c_sa_shared_ptr& inet6_address, const sockaddr* sa);
  void                set_generic_inet_address(const char* category, c_sa_shared_ptr& inet_address, const sockaddr* sa);
  void                set_generic_inet6_address(const char* category, c_sa_shared_ptr& inet6_address, const sockaddr* sa);

  mutable std::mutex  m_mutex;

  bool                m_block_ipv4{false};
  bool                m_block_ipv6{false};
  bool                m_block_ipv4in6{false};
  bool                m_block_outgoing{false};
  bool                m_prefer_ipv6{false};

  // TODO: Rename m_tos_priority.
  int                 m_priority{iptos_throughput};

  c_sa_shared_ptr     m_bind_inet_address;
  c_sa_shared_ptr     m_bind_inet6_address;
  c_sa_shared_ptr     m_local_inet_address;
  c_sa_shared_ptr     m_local_inet6_address;
  c_sa_shared_ptr     m_proxy_address;

  uint16_t            m_listen_port{0};
  uint16_t            m_override_dht_port{0};

  uint32_t            m_send_buffer_size{0};
  uint32_t            m_receive_buffer_size{0};
  int                 m_encryption_options{encryption_none};
};

inline void NetworkConfig::set_listen_port_unsafe(uint16_t port) { m_listen_port = port; }

} // namespace torrent::net

#endif
