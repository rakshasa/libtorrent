#include "config.h"

#include "socket_address.h"

#include <cstring>
#include <sys/socket.h>

#include "rak/socket_address.h"

namespace torrent {

bool
sa_is_bindable(const sockaddr* sockaddr) {
  auto bind_address = rak::socket_address::cast_from(sockaddr);

  switch (bind_address->family()) {
  case AF_INET:
    return !bind_address->sa_inet()->is_address_any() && bind_address->sa_inet()->is_port_any();
  case AF_INET6:
    return !bind_address->sa_inet6()->is_address_any() && bind_address->sa_inet6()->is_port_any();
  case AF_UNSPEC:
  default:
    return false;
  };
}

bool
sa_is_default(const sockaddr* sockaddr) {
  return sockaddr == NULL || sockaddr->sa_family == AF_UNSPEC;
}

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

std::unique_ptr<sockaddr>
sa_make_unspec() {
  std::unique_ptr<sockaddr> sa(new sockaddr);
  std::memset(sa.get(), 0, sizeof(sockaddr));
  sa.get()->sa_family = AF_UNSPEC;

  return sa;
}

std::unique_ptr<sockaddr>
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

std::unique_ptr<sockaddr>
sa_copy_inet(const sockaddr_in* sa) {
  return std::unique_ptr<sockaddr>(reinterpret_cast<sockaddr*>(sa_inet_copy(sa).release()));
}

std::unique_ptr<sockaddr>
sa_copy_inet6(const sockaddr_in6* sa) {
  return std::unique_ptr<sockaddr>(reinterpret_cast<sockaddr*>(sa_inet6_copy(sa).release()));
}

std::unique_ptr<sockaddr_in>
sa_inet_copy(const sockaddr_in* sa) {
  std::unique_ptr<sockaddr_in> result(new sockaddr_in);
  std::memcpy(result.get(), sa, sizeof(sockaddr_in));

  return result;
}

std::unique_ptr<sockaddr_in6>
sa_inet6_copy(const sockaddr_in6* sa) {
  std::unique_ptr<sockaddr_in6> result(new sockaddr_in6);
  std::memcpy(result.get(), sa, sizeof(sockaddr_in));

  return result;
}

std::string
sa_pretty_address_str(const sockaddr* sockaddr) {
  if (sockaddr == NULL)
    return "unspec";

  return rak::socket_address::cast_from(sockaddr)->pretty_address_str();
}

void
sa_inet6_clear(sockaddr_in6* sa) {
  std::memset(sa, 0, sizeof(sockaddr_in6));
  sa->sin6_family = AF_INET6;
}

void
sa_inet_mapped_inet6(const sockaddr_in* sa, sockaddr_in6* mapped) {
  uint32_t addr32[4];
  addr32[0] = 0;
  addr32[1] = 0;
  addr32[2] = htonl(0xffff);
  addr32[3] = sa->sin_addr.s_addr;

  sa_inet6_clear(mapped);

  mapped->sin6_addr = *reinterpret_cast<in6_addr*>(addr32);
  mapped->sin6_port = sa->sin_port;
}

}
