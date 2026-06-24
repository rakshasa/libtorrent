#ifndef LIBTORRENT_NET_PROXY_PROXY_HTTP_H
#define LIBTORRENT_NET_PROXY_PROXY_HTTP_H

#include "net/proxy/proxy.h"

namespace torrent::net::proxy {

class ProxyHttp : public Proxy {
public:
  ProxyHttp(const std::string& address, uint16_t port);
  ~ProxyHttp() = default;

  int                 next_action() override;

  // uint32_t            read(char* data, uint32_t size) override;
  uint32_t            write(char* data, uint32_t max_size) override;

private:
  bool                m_done{};
  std::string         m_address;
};

} // namespace torrent::net::proxy

#endif
