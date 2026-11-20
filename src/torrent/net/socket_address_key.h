// Copyright (C) 2005-2014, Jari Sundell
// All rights reserved.

#ifndef LIBTORRENT_UTILS_SOCKET_ADDRESS_KEY_H
#define LIBTORRENT_UTILS_SOCKET_ADDRESS_KEY_H

#include <array>
#include <cstring>
#include <cinttypes>
#include <cstdint>
#include <netinet/in.h>
#include <sys/socket.h>
#include <variant>

// Unique key for the socket address, excluding port numbers, etc.

// TODO: Add include files...

namespace torrent {

class socket_address_key {
public:
  // TODO: Disable default ctor?

  // socket_address_key(const sockaddr* sa) : m_sockaddr(sa) {}

  bool is_valid() const { return !std::holds_alternative<std::monostate>(m_addr); }

  // // Rename, add same family, valid inet4/6.

  // TODO: Make from_sockaddr an rvalue reference.
  static bool is_comparable_sockaddr(const sockaddr* sa);

  static socket_address_key from_sockaddr(const sockaddr* sa);
  static socket_address_key from_sin_addr(const sockaddr_in& sa);
  static socket_address_key from_sin6_addr(const sockaddr_in6& sa);

  bool operator < (const socket_address_key& sa) const;
  bool operator > (const socket_address_key& sa) const;
  bool operator == (const socket_address_key& sa) const;

private:
  using ipv6_addr = std::array<uint8_t, sizeof(in6_addr::s6_addr)>;

  std::variant<std::monostate, uint32_t, ipv6_addr> m_addr;
};

inline bool
socket_address_key::is_comparable_sockaddr(const sockaddr* sa) {
  return sa != nullptr && (sa->sa_family == AF_INET || sa->sa_family == AF_INET6);
}

// TODO: Require socket length?

inline socket_address_key
socket_address_key::from_sockaddr(const sockaddr* sa) {
  socket_address_key result;

  if (sa == nullptr)
    return result;

  switch (sa->sa_family) {
  case AF_INET:
    // Using hardware order to allow for the use of operator < to
    // sort in lexical order.
    result.m_addr = ntohl(reinterpret_cast<const struct sockaddr_in*>(sa)->sin_addr.s_addr);
    break;

  case AF_INET6: {
    ipv6_addr addr6;
    std::memcpy(addr6.data(), reinterpret_cast<const struct sockaddr_in6*>(sa)->sin6_addr.s6_addr, addr6.size());
    result.m_addr = addr6;
    break;
  }

  default:
    break;
  }

  return result;
}

inline socket_address_key
socket_address_key::from_sin_addr(const sockaddr_in& sa) {
  socket_address_key result;

  result.m_addr = ntohl(sa.sin_addr.s_addr);

  return result;
}

inline socket_address_key
socket_address_key::from_sin6_addr(const sockaddr_in6& sa) {
  socket_address_key result;

  ipv6_addr addr6;
  std::memcpy(addr6.data(), sa.sin6_addr.s6_addr, addr6.size());
  result.m_addr = addr6;

  return result;
}

inline bool
socket_address_key::operator < (const socket_address_key& sa) const {
  return m_addr < sa.m_addr;
}

inline bool
socket_address_key::operator > (const socket_address_key& sa) const {
  return m_addr > sa.m_addr;
}

inline bool
socket_address_key::operator == (const socket_address_key& sa) const {
  return m_addr == sa.m_addr;
}

} // namespace torrent

#endif
