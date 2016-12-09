#include "config.h"

#include "socket_address.h"

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

std::string
sa_pretty_address_str(const sockaddr* sockaddr) {
  if (sockaddr == NULL)
    return "unspec";

  return rak::socket_address::cast_from(sockaddr)->pretty_address_str();
}

}
