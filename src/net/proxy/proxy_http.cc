#include "config.h"

#include "net/proxy/proxy_http.h"

#include <cassert>

#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"

namespace torrent::net::proxy {

ProxyHttp::ProxyHttp(const sockaddr* proxy_sa, const std::string& host, uint16_t port)
  : m_host(host),
    m_port(port),
    m_state(state_writing){

  sa_copy_to_inet_union(proxy_sa, m_proxy_sa);

  assert(m_proxy_sa.sa.sa_family == AF_INET || m_proxy_sa.sa.sa_family == AF_INET6);
  assert(!sa_is_any(&m_proxy_sa.sa) && sa_port(&m_proxy_sa.sa) != 0);
  assert(!m_host.empty() && m_port != 0);
}

int
ProxyHttp::next_action() {
  return m_state;
}

uint32_t
ProxyHttp::write(char* data, uint32_t max_size) {
  assert(m_state == state_writing);

  static const char* part1 = "CONNECT ";
  static const char* part2 = " HTTP/1.1\r\nHost: ";
  static const char* part3 = "\r\n\r\n";

  auto host_port_str = m_host + ":" + std::to_string(m_port);

  uint32_t header_size = sizeof(part1) - 1 + host_port_str.size() + sizeof(part2) - 1 + host_port_str.size() + sizeof(part3) - 1;

  if (max_size < header_size)
    throw internal_error("ProxyHttp::write() buffer too small for proxy header.");

  auto ptr = data;

  std::memcpy(ptr, part1, sizeof(part1) - 1);
  ptr += sizeof(part1) - 1;

  std::memcpy(ptr, host_port_str.data(), host_port_str.size());
  ptr += host_port_str.size();

  std::memcpy(ptr, part2, sizeof(part2) - 1);
  ptr += sizeof(part2) - 1;

  std::memcpy(ptr, host_port_str.data(), host_port_str.size());
  ptr += host_port_str.size();

  std::memcpy(ptr, part3, sizeof(part3) - 1);
  ptr += sizeof(part3) - 1;

  assert(ptr - data == header_size);

  m_state = state_reading;
  return header_size;
}

uint32_t
ProxyHttp::read(const char* data, uint32_t size) {
  assert(m_state == state_reading);

  if (!m_verified_header) {
    if (size < 12)
      return 0;

    if (std::memcmp(data, "HTTP/1.1 200", 12) != 0 && std::memcmp(data, "HTTP/1.0 200", 12) != 0) {
      m_state = state_error;
      return 0;
    }

    m_verified_header = true;
  }

  if (size < 4)
    return 0;

  static const char* pattern = "\r\n\r\n";

  auto itr = std::search(data, data + size, pattern, pattern + 4);

  if (itr == data + size)
    return std::distance(data, itr) - 3;

  m_state = state_finished;
  return std::distance(data, itr) + 4;
}

} // namespace torrent::net::proxy
