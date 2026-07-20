#include "config.h"

#include "net/proxy/proxy_socks5.h"

#include <cassert>

#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"

namespace torrent::net::proxy {

ProxySocks5::ProxySocks5(const sockaddr* proxy_sa, const sockaddr* connect_sa, std::string user, std::string password)
  : m_user(std::move(user)),
    m_password(std::move(password)) {

  sa_copy_to_inet_union(proxy_sa,  m_proxy_address);
  sa_copy_to_inet_union(connect_sa, m_connect_address);

  m_connect_port = sa_port(connect_sa);

  assert(!sa_is_any(&m_proxy_address.sa) && sa_port(&m_proxy_address.sa) != 0);
  assert(!sa_is_any(&m_connect_address.sa) && m_connect_port != 0);

  assert(m_user.size() <= 255);
  assert(m_password.size() <= 255);
}

ProxySocks5::~ProxySocks5() = default;

int ProxySocks5::next_action() {
  return m_state;
}

uint32_t ProxySocks5::write(void* data, uint32_t max_size) {
  assert(m_state == state_writing);

  auto* ptr = static_cast<uint8_t*>(data);

  switch (m_socks_state) {
  case socks_state::greeting_write:
    if (max_size < 4)
      throw internal_error("ProxySocks5::write() buffer too small for greeting header.");

    *ptr++ = 0x05; // SOCKS5 Version

    if (!m_user.empty()) {
      *ptr++ = 2;    // 2 authentication methods offered
      *ptr++ = 0x00; // No Auth
      *ptr++ = 0x02; // User/Password Auth
    } else {
      *ptr++ = 1;    // 1 authentication method offered
      *ptr++ = 0x00; // No Auth
    }

    m_socks_state = socks_state::greeting_read;
    break;

  case socks_state::auth_write:
    if (max_size < 3 + m_user.size() + m_password.size())
      throw internal_error("ProxySocks5::write() buffer too small for auth header.");

    *ptr++ = 0x01; // Subnegotiation Version 1

    *ptr++ = static_cast<uint8_t>(m_user.size());
    std::memcpy(ptr, m_user.data(), m_user.size());
    ptr += m_user.size();

    *ptr++ = static_cast<uint8_t>(m_password.size());
    std::memcpy(ptr, m_password.data(), m_password.size());
    ptr += m_password.size();

    m_socks_state = socks_state::auth_read;
    break;

  case socks_state::connect_write:
    if (max_size < 22)
      throw internal_error("ProxySocks5::write() buffer too small for connect header.");

    *ptr++ = 0x05; // SOCKS5 Version
    *ptr++ = 0x01; // Command: CONNECT
    *ptr++ = 0x00; // Reserved

    if (m_connect_address.sa.sa_family == AF_INET) {
      *ptr++ = 0x01;
      std::memcpy(ptr, &m_connect_address.inet.sin_addr.s_addr, 4);
      ptr += 4;

    } else if (m_connect_address.sa.sa_family == AF_INET6) {
      *ptr++ = 0x04;
      std::memcpy(ptr, &m_connect_address.inet6.sin6_addr.s6_addr, 16);
      ptr += 16;

    } else {
      throw internal_error("ProxySocks5::write() connect address family is invalid.");
    }

    *ptr++ = static_cast<uint8_t>(m_connect_port >> 8);
    *ptr++ = static_cast<uint8_t>(m_connect_port & 0xFF);

    m_socks_state = socks_state::connect_read;
    break;

  default:
    throw internal_error("ProxySocks5::write() entered unexpected write sub-state.");
  }

  m_state = state_reading;
  return std::distance(static_cast<uint8_t*>(data), ptr);
}

uint32_t ProxySocks5::read(const void* data, uint32_t size) {
  assert(m_state == state_reading);

  auto* bytes = static_cast<const uint8_t*>(data);

  switch (m_socks_state) {
  case socks_state::greeting_read:
    if (size < 2)
      return 0;

    if (bytes[0] != 0x05) {
      m_state = state_error;
      return 0;
    }

    switch (bytes[1]) {
    case 0x00:
      m_state       = state_writing;
      m_socks_state = socks_state::connect_write;
      return 2;

    case 0x02:
      if (m_user.empty()) {
        m_state = state_error;
        return 0;
      }

      m_state       = state_writing;
      m_socks_state = socks_state::auth_write;
      return 2;

    default:
      m_state = state_error;
      return 0;
    }

  case socks_state::auth_read:
    if (size < 2)
      return 0;

    if (bytes[0] != 0x01 || bytes[1] != 0x00) {
      m_state = state_error;
      return 0;
    }

    m_state       = state_writing;
    m_socks_state = socks_state::connect_write;
    return 2;

  case socks_state ::connect_read:
    if (size < 2)
      return 0;

    if (bytes[0] != 0x05 || bytes[1] != 0x00) {
      m_state = state_error;
      return 0;
    }

    // The minimum size is 4 bytes (version, reply, reserved, address type) and 2 bytes for the
    // port, plus address length.
    if (size < 4 + 2)
      return 0;

    size -= 6;

    switch (bytes[3]) {
    case 0x01:
      if (size < 4)
        return 0;

      m_state = state_done;
      return 6 + 4;

    case 0x04:
      if (size < 16)
        return 0;

      m_state = state_done;
      return 6 + 16;

    case 0x03:
      if (size < 1 + static_cast<unsigned int>(bytes[4]))
        return 0;

      m_state = state_done;
      return 6 + 1 + static_cast<unsigned int>(bytes[4]);

    default:
      m_state = state_error;
      return 0;
    }

  default:
    m_state = state_error;
    return 0;
  }
}

} // namespace torrent::net::proxy
