#include "config.h"

#include "net/proxy/proxy_socks5.h"

#include <cassert>

#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"

namespace torrent::net::proxy {

ProxySocks5::ProxySocks5(const sockaddr* proxy_sa, const sockaddr* target_sa, std::string user, std::string password)
  : m_user(std::move(user)),
    m_password(std::move(password)) {

  sa_copy_to_inet_union(proxy_sa,  m_proxy_address);
  sa_copy_to_inet_union(target_sa, m_target_address);

  m_target_port = sa_port(target_sa);

  assert(!sa_is_any(&m_proxy_address.sa) && sa_port(&m_proxy_address.sa) != 0);
  assert(!sa_is_any(&m_target_address.sa) && m_target_port != 0);

  assert(m_user.size() <= 255);
  assert(m_password.size() <= 255);
}

ProxySocks5::~ProxySocks5() = default;

int ProxySocks5::next_action() {
  return m_state;
}

uint32_t ProxySocks5::write(void* data, uint32_t max_size) {
  assert(m_state == state_writing);

  if (max_size < max_header_size)
    throw internal_error("ProxySocks5::write() buffer smaller than max_header_size.");

  auto* ptr = static_cast<uint8_t*>(data);

  switch (m_socks_state) {
  case socks_state::greeting_write:
    *ptr++ = 0x05; // SOCKS5 Version

    if (!m_user.empty()) {
      *ptr++ = 2;    // 2 authentication methods offered
      *ptr++ = 0x00; // No Auth
      *ptr++ = 0x02; // User/Password Auth
    } else {
      *ptr++ = 1;    // 1 authentication method offered
      *ptr++ = 0x00; // No Auth
    }

    m_state       = state_reading;
    m_socks_state = socks_state::greeting_read;

    return std::distance(static_cast<uint8_t*>(data), ptr);

  case socks_state::auth_write:
    *ptr++ = 0x01; // Subnegotiation Version 1

    *ptr++ = static_cast<uint8_t>(m_user.size());
    std::memcpy(ptr, m_user.data(), m_user.size());
    ptr += m_user.size();

    *ptr++ = static_cast<uint8_t>(m_password.size());
    std::memcpy(ptr, m_password.data(), m_password.size());
    ptr += m_password.size();

    m_state = state_reading;
    m_socks_state = socks_state::auth_read;

    return std::distance(static_cast<uint8_t*>(data), ptr);

  case socks_state::connect_write:
    *ptr++ = 0x05; // SOCKS5 Version
    *ptr++ = 0x01; // Command: CONNECT
    *ptr++ = 0x00; // Reserved

    if (m_target_address.sa.sa_family == AF_INET) {
      *ptr++ = 0x01;
      std::memcpy(ptr, &m_target_address.inet.sin_addr.s_addr, 4);
      ptr += 4;
    } else if (m_target_address.sa.sa_family == AF_INET6) {
      *ptr++ = 0x04;
      std::memcpy(ptr, &m_target_address.inet6.sin6_addr.s6_addr, 16);
      ptr += 16;
    } else {
      throw internal_error("ProxySocks5::write() target address family is invalid.");
    }

    *ptr++ = static_cast<uint8_t>(m_target_port >> 8);
    *ptr++ = static_cast<uint8_t>(m_target_port & 0xFF);

    m_state       = state_reading;
    m_socks_state = socks_state::connect_read;

    return std::distance(static_cast<uint8_t*>(data), ptr);

  default:
    throw internal_error("ProxySocks5::write() entered unexpected write sub-state.");
  }
}

uint32_t ProxySocks5::read(const void* data, uint32_t size) {
  assert(m_state == state_reading);
  auto* bytes = static_cast<const uint8_t*>(data);

  switch (m_socks_state) {
  case socks_state::greeting_read: {
    if (size < 2)
      return 0;

    if (bytes[0] != 0x05) {
      m_state = state_error;
      return 2;
    }

    uint8_t method = bytes[1];

    if (method == 0x00) {
      m_socks_state = socks_state::connect_write;
      m_state = state_writing;
    } else if (method == 0x02 && !m_user.empty()) {
      m_socks_state = socks_state::auth_write;
      m_state = state_writing;
    } else {
      m_state = state_error;
    }

    return 2;
  }

  case socks_state::auth_read:
    if (size < 2)
      return 0;

    if (bytes[0] != 0x01 || bytes[1] != 0x00) {
      m_state = state_error;
      return 2;
    }

    m_state       = state_writing;
    m_socks_state = socks_state::connect_write;

    return 2;

  case socks_state ::connect_read: {
    if (size < 4)
      return 0;

    if (bytes[0] != 0x05 || bytes[1] != 0x00) {
      m_state = state_error;
      return size;
    }

    uint8_t  atyp                = bytes[3];
    uint32_t expected_total_size = 4; // Version (1) + Rep (1) + Rsv (1) + Atyp (1)

    if (atyp == 0x01) {       // IPv4
      expected_total_size += 4 + 2;

    } else if (atyp == 0x04) { // IPv6
      expected_total_size += 16 + 2;

    } else if (atyp == 0x03) { // Domain Name
      if (size < 5)
        return 0;

      uint8_t domain_len = bytes[4];
      expected_total_size += 1 + domain_len + 2;
    } else {
      m_state = state_error;
      return size;
    }

    if (size < expected_total_size)
      return 0;

    m_state = state_done;

    return expected_total_size;
  }

  default:
    m_state = state_error;
    return size;
  }
}

} // namespace torrent::net::proxy
