#include "config.h"

#include "socket_address.h"

#include <cstring>
#include <netinet/in.h>

// TODO: Needed?
#include <arpa/inet.h>
#include <sys/socket.h>

// TODO: Deprecate.
#include "rak/socket_address.h"

#include "torrent/exceptions.h"

namespace torrent {

// TODO: Cleanup.
inline uint32_t
sa_in6_addr32_index(const sockaddr_in6* sa, unsigned int index) {
  // TODO: Throw if >=4
  // TODO: Use proper casting+htonl.
  return
    (sa->sin6_addr.s6_addr[index * 4 + 0] << 24) +
    (sa->sin6_addr.s6_addr[index * 4 + 1] << 16) +
    (sa->sin6_addr.s6_addr[index * 4 + 2] << 8) +
    (sa->sin6_addr.s6_addr[index * 4 + 3] << 0);
}

inline void
sa_in6_addr32_set(sockaddr_in6* sa, unsigned int index, uint32_t value) {
  sa->sin6_addr.s6_addr[index * 4 + 0] = (value >> 24);
  sa->sin6_addr.s6_addr[index * 4 + 1] = (value >> 16);
  sa->sin6_addr.s6_addr[index * 4 + 2] = (value >> 8);
  sa->sin6_addr.s6_addr[index * 4 + 3] = (value >> 0);
}

inline in6_addr
sa_in6_make_addr32(uint32_t addr0, uint32_t addr1, uint32_t addr2, uint32_t addr3) {
  uint32_t addr32[4];
  addr32[0] = htonl(addr0);
  addr32[1] = htonl(addr1);
  addr32[2] = htonl(addr2);
  addr32[3] = htonl(addr3);

  return *reinterpret_cast<in6_addr*>(addr32);
}

bool
sa_is_unspec(const sockaddr* sockaddr) {
  return sockaddr != NULL && sockaddr->sa_family == AF_UNSPEC;
}

bool
sa_is_inet(const sockaddr* sockaddr) {
  return sockaddr != NULL && sockaddr->sa_family == AF_INET;
}

bool
sa_is_inet6(const sockaddr* sockaddr) {
  return sockaddr != NULL && sockaddr->sa_family == AF_INET6;
}

bool
sa_is_inet_inet6(const sockaddr* sockaddr) {
  return sockaddr != NULL && (sockaddr->sa_family == AF_INET || sockaddr->sa_family == AF_INET6);
}

bool
sa_is_any(const sockaddr* sockaddr) {
  if (sockaddr == NULL)
    return true;

  switch (sockaddr->sa_family) {
  case AF_INET:
    return sa_in_is_any(reinterpret_cast<const sockaddr_in*>(sockaddr));
  case AF_INET6:
    return sa_in6_is_any(reinterpret_cast<const sockaddr_in6*>(sockaddr));
  case AF_UNSPEC:
  default:
    return true;
  }
}

bool
sa_in_is_any(const sockaddr_in* sockaddr) {
  return sockaddr->sin_addr.s_addr == htonl(INADDR_ANY);
}

bool
sa_in6_is_any(const sockaddr_in6* sockaddr) {
  return std::memcmp(&sockaddr->sin6_addr, &in6addr_any, sizeof(in6_addr)) == 0;
}

bool
sa_is_v4mapped(const sockaddr* sockaddr) {
  return sockaddr != NULL && sockaddr->sa_family == AF_INET6 && sa_in6_is_v4mapped(reinterpret_cast<const sockaddr_in6*>(sockaddr));
}

bool
sa_in6_is_v4mapped(const sockaddr_in6* sockaddr) {
  return sockaddr != NULL && IN6_IS_ADDR_V4MAPPED(&sockaddr->sin6_addr);
}

// TODO: Technically not correct, use sa length and throw if
// invalid. Also make sure the length is not larger than required.
size_t
sa_length(const sockaddr* sa) {
  switch(sa->sa_family) {
  case AF_INET:
    return sizeof(sockaddr_in);
  case AF_INET6:
    return sizeof(sockaddr_in6);
  case AF_UNSPEC:
  default:
    return sizeof(sockaddr);
  }
}

sa_unique_ptr
sa_make_unspec() {
  sa_unique_ptr sa(new sockaddr);
  std::memset(sa.get(), 0, sizeof(sockaddr));
  sa.get()->sa_family = AF_UNSPEC;

  return sa;
}

sa_unique_ptr
sa_make_inet() {
  return sa_unique_ptr(reinterpret_cast<sockaddr*>(sa_in_make().release()));
}

sa_unique_ptr
sa_make_inet6() {
  return sa_unique_ptr(reinterpret_cast<sockaddr*>(sa_in6_make().release()));
}

sa_unique_ptr
sa_copy(const sockaddr* sa) {
  if (sa == NULL)
    return sa_make_unspec();

  switch(sa->sa_family) {
  case AF_INET:
    return sa_copy_inet(reinterpret_cast<const sockaddr_in*>(sa));
  case AF_INET6:
    return sa_copy_inet6(reinterpret_cast<const sockaddr_in6*>(sa));
  case AF_UNSPEC:
  default:
    return sa_make_unspec();
  }
}

sa_in_unique_ptr
sa_in_make() {
  sa_in_unique_ptr sa(new sockaddr_in);
  std::memset(sa.get(), 0, sizeof(sockaddr_in));
  sa.get()->sin_family = AF_INET;

  return sa;
}

sa_in6_unique_ptr
sa_in6_make() {
  sa_in6_unique_ptr sa(new sockaddr_in6);
  std::memset(sa.get(), 0, sizeof(sockaddr_in6));
  sa.get()->sin6_family = AF_INET6;

  return sa;
}

sa_unique_ptr
sa_copy_inet(const sockaddr_in* sa) {
  return sa_unique_ptr(reinterpret_cast<sockaddr*>(sa_in_copy(sa).release()));
}

sa_unique_ptr
sa_copy_inet6(const sockaddr_in6* sa) {
  return sa_unique_ptr(reinterpret_cast<sockaddr*>(sa_in6_copy(sa).release()));
}

// TODO: Use proper memset+memcpy length.
sa_in_unique_ptr
sa_in_copy(const sockaddr_in* sa) {
  sa_in_unique_ptr result(new sockaddr_in);
  std::memcpy(result.get(), sa, sizeof(sockaddr_in));

  return result;
}

sa_in6_unique_ptr
sa_in6_copy(const sockaddr_in6* sa) {
  sa_in6_unique_ptr result(new sockaddr_in6);
  std::memcpy(result.get(), sa, sizeof(sockaddr_in6));

  return result;
}

sa_unique_ptr
sa_from_v4mapped(const sockaddr* sa) {
  // TODO: This needs to check if in6. Use a safe sa cast.
  return sa_unique_ptr(reinterpret_cast<sockaddr*>(sa_in_from_in6_v4mapped(reinterpret_cast<const sockaddr_in6*>(sa)).release()));
}

sa_unique_ptr
sa_to_v4mapped(const sockaddr* sa) {
  // TODO: This needs to check if in. Use a safe sa cast.
  return sa_unique_ptr(reinterpret_cast<sockaddr*>(sa_in6_to_in_v4mapped(reinterpret_cast<const sockaddr_in*>(sa)).release()));
}

sa_in_unique_ptr
sa_in_from_in6_v4mapped(const sockaddr_in6* sa) {
  if (!sa_in6_is_v4mapped(sa))
    throw internal_error("torrent::sa_in6_is_v4mapped: sockaddr_in6 is not v4mapped");

  sa_in_unique_ptr result = sa_in_make();
  result.get()->sin_addr.s_addr = reinterpret_cast<in_addr_t>(htonl(sa_in6_addr32_index(sa, 3)));
  result.get()->sin_port = sa->sin6_port;

  return result;
}

sa_in6_unique_ptr
sa_in6_to_in_v4mapped(const sockaddr_in* sa) {
  sa_in6_unique_ptr result = sa_in6_make();

  result.get()->sin6_addr = sa_in6_make_addr32(0, 0, 0xffff, ntohl(sa->sin_addr.s_addr));
  result.get()->sin6_port = sa->sin_port;

  return result;
}

sa_in_unique_ptr
sin_from_sa(sa_unique_ptr&& sap) {
  if (!sa_is_inet(sap.get()))
    throw internal_error("torrent::sin_from_sa: sockaddr is nullptr or not inet");

  return sa_in_unique_ptr(reinterpret_cast<sockaddr_in*>(sap.release()));
}

sa_in6_unique_ptr
sin6_from_sa(sa_unique_ptr&& sap) {
  if (!sa_is_inet6(sap.get()))
    throw internal_error("torrent::sin6_from_sa: sockaddr is nullptr or not inet6");

  return sa_in6_unique_ptr(reinterpret_cast<sockaddr_in6*>(sap.release()));
}

void
sa_clear_inet6(sockaddr_in6* sa) {
  std::memset(sa, 0, sizeof(sockaddr_in6));
  sa->sin6_family = AF_INET6;
}

uint16_t
sa_port(const sockaddr* sa) {
  switch(sa->sa_family) {
  case AF_INET:
    return ntohs(reinterpret_cast<const sockaddr_in*>(sa)->sin_port);
  case AF_INET6:
    return ntohs(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_port);
  case AF_UNSPEC:
  default:
    return 0; // Do something?
  }
}

void
sa_set_port(sockaddr* sa, uint16_t port) {
  switch(sa->sa_family) {
  case AF_INET:
    reinterpret_cast<sockaddr_in*>(sa)->sin_port = htons(port);
  case AF_INET6:
    reinterpret_cast<sockaddr_in6*>(sa)->sin6_port = htons(port);
  case AF_UNSPEC:
  default:
    ; // Do something?
  }
}

bool
sa_is_port_any(const sockaddr* sa) {
  switch(sa->sa_family) {
  case AF_INET:
    return ntohs(reinterpret_cast<const sockaddr_in*>(sa)->sin_port) == 0;
  case AF_INET6:
    return ntohs(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_port) == 0;
  case AF_UNSPEC:
  default:
    return true;
  }
}

std::string
sa_addr_str(const sockaddr* sa) {
  if (sa == NULL)
    return "unspec";

  switch (sa->sa_family) {
  case AF_INET:
    return sa_in_addr_str(reinterpret_cast<const sockaddr_in*>(sa));
  case AF_INET6:
    return sa_in6_addr_str(reinterpret_cast<const sockaddr_in6*>(sa));
  case AF_UNSPEC:
    return "unspec";
  default:
    return "invalid";
  }
}

std::string
sa_in_addr_str(const sockaddr_in* sa) {
  char buffer[INET_ADDRSTRLEN];

  if (inet_ntop(AF_INET, &sa->sin_addr, buffer, INET_ADDRSTRLEN) == NULL)
    return "inet_error";

  return buffer;
}


std::string
sa_in6_addr_str(const sockaddr_in6* sa) {
  char buffer[INET6_ADDRSTRLEN];

  if (inet_ntop(AF_INET6, &sa->sin6_addr, buffer, INET6_ADDRSTRLEN) == NULL)
    return "inet6_error";

  return buffer;
}

std::string
sa_pretty_str(const sockaddr* sa) {
  if (sa == NULL)
    return "nullptr";

  switch (sa->sa_family) {
  case AF_INET:
    return sa_in_pretty_str(reinterpret_cast<const sockaddr_in*>(sa));
  case AF_INET6:
    return sa_in6_pretty_str(reinterpret_cast<const sockaddr_in6*>(sa));
  case AF_UNSPEC:
    return "unspec";
  default:
    return "invalid";
  }
}

std::string
sa_in_pretty_str(const sockaddr_in* sa) {
  auto result = sa_in_addr_str(sa);

  if (sa->sin_port != 0)
    result += ':' + std::to_string(ntohs(sa->sin_port));

  return result;
}

std::string
sa_in6_pretty_str(const sockaddr_in6* sa) {
  auto result = "[" + sa_in6_addr_str(sa) + "]";

  if (sa->sin6_port != 0)
    result += ':' + std::to_string(ntohs(sa->sin6_port));

  return result;
}

// Deprecated:

void
sa_inet_mapped_inet6(const sockaddr_in* sa, sockaddr_in6* mapped) {
  uint32_t addr32[4];
  addr32[0] = 0;
  addr32[1] = 0;
  addr32[2] = htonl(0xffff);
  addr32[3] = sa->sin_addr.s_addr;

  sa_clear_inet6(mapped);

  mapped->sin6_addr = *reinterpret_cast<in6_addr*>(addr32);
  mapped->sin6_port = sa->sin_port;
}

std::string
sa_pretty_address_str(const sockaddr* sockaddr) {
  return sa_pretty_str(sockaddr);
}

}
