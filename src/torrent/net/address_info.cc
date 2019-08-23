#include "config.h"

#include "address_info.h"

namespace torrent {

int
ai_get_addrinfo(const char* nodename, const char* servname, const addrinfo* hints, ai_unique_ptr& res) {
  addrinfo* ai;
  int err = ::getaddrinfo(nodename, servname, hints, &ai);

  if (err != 0)
    return err;

  res.reset(ai);
  return 0;
}

sa_unique_ptr
ai_get_first_sa(const char* nodename, const char* servname, const addrinfo* hints) {
  ai_unique_ptr aip;

  if (ai_get_addrinfo(nodename, servname, hints, aip) != 0)
    return nullptr;

  return sa_copy(aip->ai_addr);
}

int
ai_each_inet_inet6_first(const char* nodename, ai_sockaddr_func lambda) {
  int err;
  ai_unique_ptr ai;

  // TODO: Change to a single call using hints with both inet/inet6.
  if ((err = ai_get_addrinfo(nodename, NULL, ai_make_hint(0, PF_INET, SOCK_STREAM).get(), ai)) != 0 &&
      (err = ai_get_addrinfo(nodename, NULL, ai_make_hint(0, PF_INET6, SOCK_STREAM).get(), ai)) != 0)
    return err;

  lambda(ai->ai_addr);
  return 0;
}

}
