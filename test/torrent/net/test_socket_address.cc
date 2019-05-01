#include "config.h"

#include "test_socket_address.h"

#include <algorithm>
#include <iterator>

#include "helpers/address_info.h"

#include "torrent/net/socket_address.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_socket_address);

void
test_socket_address::test_make() {
  torrent::sa_unique_ptr sa_unspec = torrent::sa_make_unspec();

  CPPUNIT_ASSERT(sa_unspec != nullptr);
  CPPUNIT_ASSERT(sa_unspec->sa_family == AF_UNSPEC);

  // TODO: Add test to convert between unique ptr's.
  torrent::sa_unique_ptr sa_inet = torrent::sa_make_inet();
  sockaddr_in* sin_inet = reinterpret_cast<sockaddr_in*>(sa_inet.get());

  CPPUNIT_ASSERT(sa_inet != nullptr);
  CPPUNIT_ASSERT(sa_inet->sa_family == AF_INET);
  CPPUNIT_ASSERT(sin_inet->sin_family == AF_INET);
  CPPUNIT_ASSERT(sin_inet->sin_port == 0);
  CPPUNIT_ASSERT(sin_inet->sin_addr.s_addr == in_addr().s_addr);

  torrent::sa_unique_ptr sa_inet6 = torrent::sa_make_inet6();
  sockaddr_in6* sin6_inet6 = reinterpret_cast<sockaddr_in6*>(sa_inet6.get());

  CPPUNIT_ASSERT(sa_inet6 != nullptr);
  CPPUNIT_ASSERT(sa_inet6->sa_family == AF_INET6);
  CPPUNIT_ASSERT(sin6_inet6->sin6_family == AF_INET6);
  CPPUNIT_ASSERT(sin6_inet6->sin6_port == 0);
  CPPUNIT_ASSERT(sin6_inet6->sin6_flowinfo == 0);
  CPPUNIT_ASSERT(std::equal(sin6_inet6->sin6_addr.s6_addr, sin6_inet6->sin6_addr.s6_addr + 16, in6_addr{0}.s6_addr));
  CPPUNIT_ASSERT(sin6_inet6->sin6_scope_id == 0);
}

void
test_socket_address::test_sa_in_compare() {
}
