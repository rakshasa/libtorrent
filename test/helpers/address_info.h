#ifndef LIBTORRENT_HELPER_ADDRESS_INFO_H
#define LIBTORRENT_HELPER_ADDRESS_INFO_H

#include <string>
#include <cppunit/extensions/HelperMacros.h>

#include "torrent/net/address_info.h"

inline torrent::sa_unique_ptr
wrap_ai_get_first_sa(const char* nodename, const char* servname = nullptr, const addrinfo* hints = nullptr) {
  auto sa = torrent::ai_get_first_sa(nodename, servname, hints);

  CPPUNIT_ASSERT_MESSAGE(("wrap_ai_get_first_sa: nodename:'" + std::string(nodename) + "'").c_str(),
                        sa != nullptr);
  return sa;
}

#endif
