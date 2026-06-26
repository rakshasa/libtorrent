#ifndef LIBTORRENT_NET_PROXY_PROXY_HTTP_H
#define LIBTORRENT_NET_PROXY_PROXY_HTTP_H

#include "net/proxy/proxy.h"

namespace torrent::net::proxy {

class ProxyHttp : public Proxy {
public:
  ProxyHttp(const sockaddr* proxy_sa, const std::string& host, uint16_t port);
  ~ProxyHttp() = default;

  int                 next_action() override;

  uint32_t            write(void* data, uint32_t max_size) override;
  uint32_t            read(const void* data, uint32_t size) override;

private:
  std::string         m_host;
  uint16_t            m_port{};

  int                 m_state{};
  bool                m_verified_header{};
};

} // namespace torrent::net::proxy

#endif
