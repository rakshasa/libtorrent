#ifndef LIBTORRENT_TORRENT_NET_NETWORK_CONFIG_H
#define LIBTORRENT_TORRENT_NET_NETWORK_CONFIG_H

#include <mutex>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <torrent/net/types.h>

namespace torrent::net {

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

  // TODO: Move helper functions in rtorrent manager here.

  // TODO: Verify we attempt to connect to cached peers on startup, even before tracker requests.

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

  std::tuple<c_sa_shared_ptr, c_sa_shared_ptr> bind_addresses_or_null() const;
  std::tuple<std::string, std::string>         bind_addresses_str() const;

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

  uint32_t            encryption_options() const;
  void                set_encryption_options(uint32_t opts);

  int                 listen_backlog() const;
  void                set_listen_backlog(int backlog);

  uint16_t            override_dht_port() const;
  void                set_override_dht_port(uint16_t port);

  uint32_t            send_buffer_size() const;
  void                set_send_buffer_size(uint32_t s);

  uint32_t            receive_buffer_size() const;
  void                set_receive_buffer_size(uint32_t s);

protected:
  friend class torrent::ConnectionManager;
  friend class torrent::net::NetworkManager;

  typedef std::tuple<c_sa_shared_ptr, c_sa_shared_ptr, bool> listen_addresses;

  void                lock() const                    { m_mutex.lock(); }
  auto                lock_guard() const              { return std::lock_guard(m_mutex); }
  void                unlock() const                  { m_mutex.unlock(); }
  auto&               mutex() const                   { return m_mutex; }

  listen_addresses    listen_addresses_unsafe();
  int                 listen_backlog_unsafe() const;

private:
  c_sa_shared_ptr     generic_address_best_match(const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const;
  std::string         generic_address_best_match_str(const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const;
  c_sa_shared_ptr     generic_address_or_unspec_and_null(const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const;
  c_sa_shared_ptr     generic_address_or_any_and_null(const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const;
  c_sa_shared_ptr     generic_address_for_connect(int family, const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const;
  c_sa_shared_ptr     generic_address_for_family(int family, const c_sa_shared_ptr& inet_address, const c_sa_shared_ptr& inet6_address) const;

  void                set_generic_address_unsafe(const char* category, c_sa_shared_ptr& inet_address, c_sa_shared_ptr& inet6_address, const sockaddr* sa);
  void                set_generic_inet_address_unsafe(const char* category, c_sa_shared_ptr& inet_address, const sockaddr* sa);
  void                set_generic_inet6_address_unsafe(const char* category, c_sa_shared_ptr& inet6_address, const sockaddr* sa);

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

  int                 m_encryption_options{encryption_none};
  int                 m_listen_backlog{SOMAXCONN};
  uint16_t            m_override_dht_port{0};
  uint32_t            m_send_buffer_size{0};
  uint32_t            m_receive_buffer_size{0};
};

} // namespace torrent::net

#endif
