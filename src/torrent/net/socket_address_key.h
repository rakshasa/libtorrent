// Copyright (C) 2005-2014, Jari Sundell
// All rights reserved.

#ifndef LIBTORRENT_UTILS_SOCKET_ADDRESS_KEY_H
#define LIBTORRENT_UTILS_SOCKET_ADDRESS_KEY_H

#include <cstring>
#include <inttypes.h>
#include <netinet/in.h>

// Unique key for the socket address, excluding port numbers, etc.

// TODO: Add include files...

namespace torrent {

class socket_address_key {
public:
  // TODO: Disable default ctor?

  // socket_address_key(const sockaddr* sa) : m_sockaddr(sa) {}

  bool is_valid() const { return m_family != AF_UNSPEC; }

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
  sa_family_t m_family;

  union {
    in_addr m_addr;
    in6_addr m_addr6;
  };
} __attribute__ ((packed));

inline bool
socket_address_key::is_comparable_sockaddr(const sockaddr* sa) {
  return sa != NULL && (sa->sa_family == AF_INET || sa->sa_family == AF_INET6);
}

// TODO: Require socket length?

inline socket_address_key
socket_address_key::from_sockaddr(const sockaddr* sa) {
  socket_address_key result;

  std::memset(&result, 0, sizeof(socket_address_key));

  result.m_family = AF_UNSPEC;

  if (sa == NULL)
    return result;

  switch (sa->sa_family) {
  case AF_INET:
    // Using hardware order to allo for the use of operator < to
    // sort in lexical order.
    result.m_family = AF_INET;
    result.m_addr.s_addr = ntohl(reinterpret_cast<const struct sockaddr_in*>(sa)->sin_addr.s_addr);
    break;

  case AF_INET6:
    result.m_family = AF_INET6;
    result.m_addr6 = reinterpret_cast<const struct sockaddr_in6*>(sa)->sin6_addr;
    break;
   
  default:
    break;
  }

  return result;
}

inline socket_address_key
socket_address_key::from_sin_addr(const sockaddr_in& sa) {
  socket_address_key result;

  std::memset(&result, 0, sizeof(socket_address_key));

  result.m_family = AF_INET;
  result.m_addr.s_addr = ntohl(sa.sin_addr.s_addr);

  return result;
}

inline socket_address_key
socket_address_key::from_sin6_addr(const sockaddr_in6& sa) {
  socket_address_key result;

  std::memset(&result, 0, sizeof(socket_address_key));

  result.m_family = AF_INET6;
  result.m_addr6 = sa.sin6_addr;

  return result;
}

inline bool
socket_address_key::operator < (const socket_address_key& sa) const {
  return std::memcmp(this, &sa, sizeof(socket_address_key)) < 0;
}

inline bool
socket_address_key::operator > (const socket_address_key& sa) const {
  return std::memcmp(this, &sa, sizeof(socket_address_key)) > 0;
}

inline bool
socket_address_key::operator == (const socket_address_key& sa) const {
  return std::memcmp(this, &sa, sizeof(socket_address_key)) == 0;
}

}

#endif
