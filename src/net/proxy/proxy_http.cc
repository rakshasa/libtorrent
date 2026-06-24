#include "config.h"

#include "net/proxy/proxy_http.h"

#include <cassert>

#include "torrent/exceptions.h"

namespace torrent::net::proxy {

ProxyHttp::ProxyHttp(const std::string& address, uint16_t port) {
  if (port != 0)
    m_address = address + ":" + std::to_string(port);
  else
    m_address = address;
}

int
ProxyHttp::next_action() {
  if (m_done)
    return state_finished;

  return state_writing;
}

uint32_t
ProxyHttp::write(char* data, uint32_t max_size) {
  static const char* proxy_header_pre  = "CONNECT ";
  static const char* proxy_header_post = " HTTP/1.0\r\n\r\n";

  uint32_t header_size = sizeof(proxy_header_pre) - 1 + m_address.size() + sizeof(proxy_header_post) - 1;

  if (max_size < header_size)
    throw internal_error("ProxyHttp::write() buffer too small for proxy header.");

  auto ptr = data;

  std::memcpy(ptr, proxy_header_pre, sizeof(proxy_header_pre) - 1);
  ptr += sizeof(proxy_header_pre) - 1;

  std::memcpy(ptr, m_address.c_str(), m_address.size());
  ptr += m_address.size();

  std::memcpy(ptr, proxy_header_post, sizeof(proxy_header_post) - 1);
  ptr += sizeof(proxy_header_post) - 1;

  assert(ptr - data == header_size);

  m_done = true;
  return header_size;
}

} // namespace torrent::net::proxy
