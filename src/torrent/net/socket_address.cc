#include "config.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <sys/un.h>

#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"

namespace torrent {

static constexpr uint32_t
sin6_addr32_index(const sockaddr_in6* sa, unsigned int index) {
  return
    (sa->sin6_addr.s6_addr[index * 4 + 0] << 24) +
    (sa->sin6_addr.s6_addr[index * 4 + 1] << 16) +
    (sa->sin6_addr.s6_addr[index * 4 + 2] << 8) +
    (sa->sin6_addr.s6_addr[index * 4 + 3] << 0);
}

static void
sin6_addr32_set(sockaddr_in6* sa, unsigned int index, uint32_t value) {
  sa->sin6_addr.s6_addr[index * 4 + 0] = (value >> 24);
  sa->sin6_addr.s6_addr[index * 4 + 1] = (value >> 16);
  sa->sin6_addr.s6_addr[index * 4 + 2] = (value >> 8);
  sa->sin6_addr.s6_addr[index * 4 + 3] = (value >> 0);
}

static in6_addr
sin6_make_addr32(uint32_t addr0, uint32_t addr1, uint32_t addr2, uint32_t addr3) {
  uint32_t addr32[4];
  addr32[0] = htonl(addr0);
  addr32[1] = htonl(addr1);
  addr32[2] = htonl(addr2);
  addr32[3] = htonl(addr3);

  return *reinterpret_cast<in6_addr*>(addr32);
}

bool
sa_is_unspec(const sockaddr* sa) {
  return sa != NULL && sa->sa_family == AF_UNSPEC;
}

bool
sa_is_inet(const sockaddr* sa) {
  return sa != NULL && sa->sa_family == AF_INET;
}

bool
sa_is_inet6(const sockaddr* sa) {
  return sa != NULL && sa->sa_family == AF_INET6;
}

bool
sa_is_inet_inet6(const sockaddr* sa) {
  return sa != NULL && (sa->sa_family == AF_INET || sa->sa_family == AF_INET6);
}

bool
sa_is_any(const sockaddr* sa) {
  switch (sa->sa_family) {
  case AF_INET:
    return sin_is_any(reinterpret_cast<const sockaddr_in*>(sa));
  case AF_INET6:
    if (sa_is_v4mapped(sa))
      return sin6_addr32_index(reinterpret_cast<const sockaddr_in6*>(sa), 3) == htonl(INADDR_ANY);
    return sin6_is_any(reinterpret_cast<const sockaddr_in6*>(sa));
  default:
    return true;
  }
}

bool
sin_is_any(const sockaddr_in* sa) {
  return sa->sin_addr.s_addr == htonl(INADDR_ANY);
}

bool
sin6_is_any(const sockaddr_in6* sa) {
  return std::memcmp(&sa->sin6_addr, &in6addr_any, sizeof(in6_addr)) == 0;
}

bool
sa_is_broadcast(const sockaddr* sa) {
  switch (sa->sa_family) {
  case AF_INET:
    return sin_is_broadcast(reinterpret_cast<const sockaddr_in*>(sa));
  case AF_INET6:
    if (sa_is_v4mapped(sa))
      return sin6_addr32_index(reinterpret_cast<const sockaddr_in6*>(sa), 3) == htonl(INADDR_BROADCAST);
    return false;
  default:
    return false;
  }
}

bool
sin_is_broadcast(const sockaddr_in* sa) {
  return sa->sin_addr.s_addr == htonl(INADDR_BROADCAST);
}

bool
sa_is_v4mapped(const sockaddr* sa) {
  return sa != NULL && sa->sa_family == AF_INET6 && sin6_is_v4mapped(reinterpret_cast<const sockaddr_in6*>(sa));
}

bool
sin6_is_v4mapped(const sockaddr_in6* sa) {
  return sa != NULL && IN6_IS_ADDR_V4MAPPED(&sa->sin6_addr);
}

bool
sa_is_port_any(const sockaddr* sa) {
  return sa_port(sa) == 0;
}

size_t
sa_length(const sockaddr* sa) {
  switch(sa->sa_family) {
  case AF_UNSPEC:
    return sizeof(sockaddr);
  case AF_INET:
    return sizeof(sockaddr_in);
  case AF_INET6:
    return sizeof(sockaddr_in6);
  case AF_UNIX:
    return sizeof(sockaddr_un);
  default:
    throw internal_error("torrent::sa_length: sockaddr is not a valid family");
  }
}

sa_unique_ptr
sa_make_unspec() {
  sa_unique_ptr sa(new sockaddr{});

  sa->sa_family = AF_UNSPEC;

  return sa;
}

sa_unique_ptr
sa_make_inet() {
  return sa_unique_ptr(reinterpret_cast<sockaddr*>(sin_make().release()));
}

sa_unique_ptr
sa_make_inet6() {
  return sa_unique_ptr(reinterpret_cast<sockaddr*>(sin6_make().release()));
}

sa_unique_ptr
sa_make_unix(const std::string& pathname) {
  if (!pathname.empty())
    throw internal_error("torrent::sa_make_unix: function not implemented");

  sun_unique_ptr sunp(new sockaddr_un{});

  sunp->sun_family = AF_UNIX;
  // TODO: verify length, copy pathname

  return sa_unique_ptr(reinterpret_cast<sockaddr*>(sunp.release()));
}

sa_unique_ptr
sa_convert(const sockaddr* sa) {
  if (sa == NULL)
    return sa_make_unspec();

  switch(sa->sa_family) {
  case AF_INET:
    return sa_copy_in(reinterpret_cast<const sockaddr_in*>(sa));
  case AF_INET6:
    if (sin6_is_v4mapped(reinterpret_cast<const sockaddr_in6*>(sa)))
      return sa_from_v4mapped_in6(reinterpret_cast<const sockaddr_in6*>(sa));

    return sa_copy_in6(reinterpret_cast<const sockaddr_in6*>(sa));
  case AF_UNSPEC:
    return sa_make_unspec();
  default:
    throw internal_error("torrent::sa_convert: sockaddr is not a valid family");
  }
}

sa_unique_ptr
sa_copy(const sockaddr* sa) {
  if (sa == nullptr)
    throw internal_error("torrent::sa_copy: sockaddr is a nullptr");

  switch(sa->sa_family) {
  case AF_INET:
    return sa_copy_in(reinterpret_cast<const sockaddr_in*>(sa));
  case AF_INET6:
    return sa_copy_in6(reinterpret_cast<const sockaddr_in6*>(sa));
  case AF_UNSPEC:
    return sa_make_unspec();
  default:
    throw internal_error("torrent::sa_copy: sockaddr is not a valid family");
  }
}

sa_unique_ptr
sa_copy_in(const sockaddr_in* sa) {
  sa_unique_ptr result(reinterpret_cast<sockaddr*>(new sockaddr_in));
  std::memcpy(result.get(), sa, sizeof(sockaddr_in));
  return result;
}

sa_unique_ptr
sa_copy_in6(const sockaddr_in6* sa) {
  sa_unique_ptr result(reinterpret_cast<sockaddr*>(new sockaddr_in6));
  std::memcpy(result.get(), sa, sizeof(sockaddr_in6));
  return result;
}

sa_unique_ptr
sa_copy_addr(const sockaddr* sa, uint16_t port) {
  if (sa == nullptr)
    throw internal_error("torrent::sa_copy_addr: sockaddr is a nullptr");

  switch(sa->sa_family) {
  case AF_INET:
    return sa_copy_addr_in(reinterpret_cast<const sockaddr_in*>(sa), port);
  case AF_INET6:
    return sa_copy_addr_in6(reinterpret_cast<const sockaddr_in6*>(sa), port);
  case AF_UNSPEC:
    return sa_make_unspec();
  default:
    throw internal_error("torrent::sa_copy_addr: sockaddr is not a valid family");
  }
}

sa_unique_ptr
sa_copy_addr_in(const sockaddr_in* sa, uint16_t port) {
  sa_unique_ptr result(reinterpret_cast<sockaddr*>(new sockaddr_in{}));
  reinterpret_cast<sockaddr_in*>(result.get())->sin_family = AF_INET;
  reinterpret_cast<sockaddr_in*>(result.get())->sin_addr = sa->sin_addr;
  reinterpret_cast<sockaddr_in*>(result.get())->sin_port = htons(port);
  return result;
}

sa_unique_ptr
sa_copy_addr_in6(const sockaddr_in6* sa, uint16_t port) {
  sa_unique_ptr result(reinterpret_cast<sockaddr*>(new sockaddr_in6{}));
  reinterpret_cast<sockaddr_in6*>(result.get())->sin6_family = AF_INET6;
  std::memcpy(&reinterpret_cast<sockaddr_in6*>(result.get())->sin6_addr, &sa->sin6_addr, sizeof(in6_addr));
  reinterpret_cast<sockaddr_in6*>(result.get())->sin6_port = htons(port);
  return result;
}

sin_unique_ptr
sin_copy(const sockaddr_in* sa) {
  sin_unique_ptr result(new sockaddr_in);
  std::memcpy(result.get(), sa, sizeof(sockaddr_in));
  return result;
}

sin6_unique_ptr
sin6_copy(const sockaddr_in6* sa) {
  sin6_unique_ptr result(new sockaddr_in6);
  std::memcpy(result.get(), sa, sizeof(sockaddr_in6));
  return result;
}

void
sa_free(const sockaddr* sa) {
  if (sa == nullptr)
    return;

  switch (sa->sa_family) {
  case AF_UNSPEC:
    delete reinterpret_cast<const sockaddr*>(sa);
    break;
  case AF_INET:
    delete reinterpret_cast<const sockaddr_in*>(sa);
    break;
  case AF_INET6:
    delete reinterpret_cast<const sockaddr_in6*>(sa);
    break;
  case AF_UNIX:
    delete reinterpret_cast<const sockaddr_un*>(sa);
    break;
  default:
    throw internal_error("torrent::sa_free: invalid family type");
  }
}

sin_unique_ptr
sin_make() {
  sin_unique_ptr sa(new sockaddr_in{});

  sa->sin_family = AF_INET;

  return sa;
}

sin6_unique_ptr
sin6_make() {
  sin6_unique_ptr sa(new sockaddr_in6{});

  sa->sin6_family = AF_INET6;

  return sa;
}

sa_unique_ptr
sa_from_v4mapped(const sockaddr* sa) {
  if (!sa_is_inet6(sa))
    throw internal_error("torrent::sa_from_v4mapped: sockaddr is not inet6");

  return sa_from_in(sin_from_v4mapped_in6(reinterpret_cast<const sockaddr_in6*>(sa)));
}

sa_unique_ptr
sa_to_v4mapped(const sockaddr* sa) {
  if (!sa_is_inet(sa))
    throw internal_error("torrent::sa_to_v4mapped: sockaddr is not inet");

  return sa_from_in6(sin6_to_v4mapped_in(reinterpret_cast<const sockaddr_in*>(sa)));
}

sin_unique_ptr
sin_from_v4mapped_in6(const sockaddr_in6* sin6) {
  if (!sin6_is_v4mapped(sin6))
    throw internal_error("torrent::sin6_is_v4mapped: sockaddr_in6 is not v4mapped");

  sin_unique_ptr result = sin_make();
  result->sin_addr.s_addr = reinterpret_cast<in_addr_t>(htonl(sin6_addr32_index(sin6, 3)));
  result->sin_port = sin6->sin6_port;

  return result;
}

sin6_unique_ptr
sin6_to_v4mapped_in(const sockaddr_in* sin) {
  sin6_unique_ptr result = sin6_make();

  result->sin6_addr = sin6_make_addr32(0, 0, 0xffff, ntohl(sin->sin_addr.s_addr));
  result->sin6_port = sin->sin_port;

  return result;
}

sin_unique_ptr
sin_from_sa(sa_unique_ptr sap) {
  if (!sap_is_inet(sap))
    throw internal_error("torrent::sin_from_sa: sockaddr is nullptr or not inet");

  return sin_unique_ptr(reinterpret_cast<sockaddr_in*>(sap.release()));
}

sin6_unique_ptr
sin6_from_sa(sa_unique_ptr sap) {
  if (!sap_is_inet6(sap))
    throw internal_error("torrent::sin6_from_sa: sockaddr is nullptr or not inet6");

  return sin6_unique_ptr(reinterpret_cast<sockaddr_in6*>(sap.release()));
}

c_sin_unique_ptr
sin_from_c_sa(c_sa_unique_ptr sap) {
  if (!sap_is_inet(sap))
    throw internal_error("torrent::sin_from_c_sa: sockaddr is nullptr or not inet");

  return c_sin_unique_ptr(reinterpret_cast<const sockaddr_in*>(sap.release()));
}

c_sin6_unique_ptr
sin6_from_c_sa(c_sa_unique_ptr sap) {
  if (!sap_is_inet6(sap))
    throw internal_error("torrent::sin6_from_c_sa: sockaddr is nullptr or not inet6");

  return c_sin6_unique_ptr(reinterpret_cast<const sockaddr_in6*>(sap.release()));
}

void
sa_clear_inet6(sockaddr_in6* sa) {
  *sa = sockaddr_in6{};
  sa->sin6_family = AF_INET6;
}

uint16_t
sa_port(const sockaddr* sa) {
  if (sa == NULL)
    return 0;

  switch(sa->sa_family) {
  case AF_INET:
    return ntohs(reinterpret_cast<const sockaddr_in*>(sa)->sin_port);
  case AF_INET6:
    return ntohs(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_port);
  default:
    return 0;
  }
}

void
sa_set_port(sockaddr* sa, uint16_t port) {
  switch(sa->sa_family) {
  case AF_INET:
    reinterpret_cast<sockaddr_in*>(sa)->sin_port = htons(port);
    return;
  case AF_INET6:
    reinterpret_cast<sockaddr_in6*>(sa)->sin6_port = htons(port);
    return;
  default:
    throw internal_error("torrent::sa_set_port: invalid family type");
  }
}

bool
sa_equal(const sockaddr* lhs, const sockaddr* rhs) {
  switch(rhs->sa_family) {
  case AF_INET:
  case AF_INET6:
  case AF_UNSPEC:
    break;
  default:
    throw internal_error("torrent::sa_equal: rhs sockaddr is not a valid family");
  }

  switch(lhs->sa_family) {
  case AF_INET:
    return lhs->sa_family == rhs->sa_family &&
      sin_equal(reinterpret_cast<const sockaddr_in*>(lhs), reinterpret_cast<const sockaddr_in*>(rhs));
  case AF_INET6:
    return lhs->sa_family == rhs->sa_family &&
      sin6_equal(reinterpret_cast<const sockaddr_in6*>(lhs), reinterpret_cast<const sockaddr_in6*>(rhs));
  case AF_UNSPEC:
    return lhs->sa_family == rhs->sa_family;
  default:
    throw internal_error("torrent::sa_equal: lhs sockaddr is not a valid family");
  }
}

bool
sin_equal(const sockaddr_in* lhs, const sockaddr_in* rhs) {
  return lhs->sin_port == rhs->sin_port && lhs->sin_addr.s_addr == rhs->sin_addr.s_addr;
}

bool
sin6_equal(const sockaddr_in6* lhs, const sockaddr_in6* rhs) {
  return lhs->sin6_port == rhs->sin6_port && std::equal(lhs->sin6_addr.s6_addr, lhs->sin6_addr.s6_addr + 16, rhs->sin6_addr.s6_addr);
}

bool
sa_equal_addr(const sockaddr* lhs, const sockaddr* rhs) {
  switch(rhs->sa_family) {
  case AF_INET:
  case AF_INET6:
  case AF_UNSPEC:
    break;
  default:
    throw internal_error("torrent::sa_equal_addr: rhs sockaddr is not a valid family");
  }

  switch(lhs->sa_family) {
  case AF_INET:
    return lhs->sa_family == rhs->sa_family &&
      sin_equal_addr(reinterpret_cast<const sockaddr_in*>(lhs), reinterpret_cast<const sockaddr_in*>(rhs));
  case AF_INET6:
    return lhs->sa_family == rhs->sa_family &&
      sin6_equal_addr(reinterpret_cast<const sockaddr_in6*>(lhs), reinterpret_cast<const sockaddr_in6*>(rhs));
  case AF_UNSPEC:
    return lhs->sa_family == rhs->sa_family;
  default:
    throw internal_error("torrent::sa_equal_addr: lhs sockaddr is not a valid family");
  }
}

bool
sin_equal_addr(const sockaddr_in* lhs, const sockaddr_in* rhs) {
  return lhs->sin_addr.s_addr == rhs->sin_addr.s_addr;
}

bool
sin6_equal_addr(const sockaddr_in6* lhs, const sockaddr_in6* rhs) {
  return std::equal(lhs->sin6_addr.s6_addr, lhs->sin6_addr.s6_addr + 16, rhs->sin6_addr.s6_addr);
}

std::string
sa_addr_str(const sockaddr* sa) {
  if (sa == NULL)
    return "unspec";

  switch (sa->sa_family) {
  case AF_INET:
    return sin_addr_str(reinterpret_cast<const sockaddr_in*>(sa));
  case AF_INET6:
    return sin6_addr_str(reinterpret_cast<const sockaddr_in6*>(sa));
  case AF_UNSPEC:
    return "unspec";
  default:
    return "invalid";
  }
}

std::string
sin_addr_str(const sockaddr_in* sa) {
  char buffer[INET_ADDRSTRLEN];

  if (inet_ntop(AF_INET, &sa->sin_addr, buffer, INET_ADDRSTRLEN) == NULL)
    return "inet_error";

  return buffer;
}


std::string
sin6_addr_str(const sockaddr_in6* sa) {
  char buffer[INET6_ADDRSTRLEN];

  if (inet_ntop(AF_INET6, &sa->sin6_addr, buffer, INET6_ADDRSTRLEN) == NULL)
    return "inet6_error";

  return buffer;
}

std::string
sa_pretty_str(const sockaddr* sa) {
  if (sa == nullptr)
    return "nullptr";

  switch (sa->sa_family) {
  case AF_INET:
    return sin_pretty_str(reinterpret_cast<const sockaddr_in*>(sa));
  case AF_INET6:
    return sin6_pretty_str(reinterpret_cast<const sockaddr_in6*>(sa));
  case AF_UNSPEC:
    return "unspec";
  default:
    return "invalid";
  }
}

std::string
sin_pretty_str(const sockaddr_in* sa) {
  auto result = sin_addr_str(sa);

  if (sa->sin_port != 0)
    result += ':' + std::to_string(ntohs(sa->sin_port));

  return result;
}

std::string
sin6_pretty_str(const sockaddr_in6* sa) {
  auto result = "[" + sin6_addr_str(sa) + "]";

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
sa_pretty_address_str(const sockaddr* sa) {
  return sa_pretty_str(sa);
}

} // namespace torrent
