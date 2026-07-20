#ifndef TORRENT_NET_PROXY_PROXY_SOCKS5_H
#define TORRENT_NET_PROXY_PROXY_SOCKS5_H

#include "net/proxy/proxy.h"

#include <string>

namespace torrent::net::proxy {

class ProxySocks5 : public Proxy {
public:
  ProxySocks5(const sockaddr* proxy_sa, const sockaddr* connect_sa,
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

  sa_inet_union       m_connect_address{};
  uint16_t            m_connect_port{};

  std::string         m_user;
  std::string         m_password;

  int                 m_state{state_writing};
  socks_state         m_socks_state{socks_state::greeting_write};
};

} // namespace torrent::net::proxy

#endif
