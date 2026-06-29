#ifndef TORRENT_NET_PROXY_PROXY_SOCKS5_H
#define TORRENT_NET_PROXY_PROXY_SOCKS5_H

#include "net/proxy/proxy.h"

#include <string>

namespace torrent::net::proxy {

class ProxySocks5 : public Proxy {
public:
  ProxySocks5(const sockaddr* proxy_sa, const sockaddr* target_sa,
              std::string user = "", std::string password = "");
  ~ProxySocks5() override;

  int                 next_action() override;

  uint32_t            write(void* data, uint32_t max_size) override;
  uint32_t            read(const void* data, uint32_t size) override;

private:
  enum class socks_state {
    greeting_write,
    greeting_read,
    auth_write,
    auth_read,
    connect_write,
    connect_read
  };

  // Max cumulative size if writes append sequentially without resetting the protocol buffer:
  //
  // Greeting (4) + Auth (513) + IPv6 CONNECT (22) = 539 bytes.
  static constexpr uint32_t max_header_size = 539;

  sa_inet_union       m_target_address{};
  uint16_t            m_target_port{};

  std::string         m_user;
  std::string         m_password;

  int                 m_state{state_writing};
  socks_state         m_socks_state{socks_state::greeting_write};
};

} // namespace torrent::net::proxy

#endif
