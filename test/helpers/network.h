#ifndef LIBTORRENT_HELPER_NETWORK_H
#define LIBTORRENT_HELPER_NETWORK_H

#include <string>
#include <cppunit/extensions/HelperMacros.h>

#include "torrent/net/address_info.h"

inline bool
compare_sin6_addr(in6_addr lhs, in6_addr rhs) {
  return std::equal(lhs.s6_addr, lhs.s6_addr + 16, rhs.s6_addr);
}

inline torrent::sa_unique_ptr
wrap_ai_get_first_sa(const char* nodename, const char* servname = nullptr, const addrinfo* hints = nullptr) {
  auto sa = torrent::ai_get_first_sa(nodename, servname, hints);

  CPPUNIT_ASSERT_MESSAGE(("wrap_ai_get_first_sa: nodename:'" + std::string(nodename) + "'").c_str(),
                        sa != nullptr);
  return sa;
}

#endif
