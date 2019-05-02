#include "config.h"

#include "test_socket_address.h"

#include "helpers/network.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_socket_address);

void
test_socket_address::test_make() {
  torrent::sa_unique_ptr sa_unspec = torrent::sa_make_unspec();

  CPPUNIT_ASSERT(sa_unspec != nullptr);
  CPPUNIT_ASSERT(sa_unspec->sa_family == AF_UNSPEC);

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
  CPPUNIT_ASSERT(compare_sin6_addr(sin6_inet6->sin6_addr, in6_addr{0}));
  CPPUNIT_ASSERT(sin6_inet6->sin6_scope_id == 0);
}

void
test_socket_address::test_sin_from_sa() {
  torrent::sa_unique_ptr sa_inet = wrap_ai_get_first_sa("1.2.3.4");
  torrent::sa_in_unique_ptr sin_inet;

  CPPUNIT_ASSERT(sa_inet != nullptr);
  CPPUNIT_ASSERT_NO_THROW({ sin_inet = torrent::sin_from_sa(std::move(sa_inet)); });
  CPPUNIT_ASSERT(sa_inet == nullptr);
  CPPUNIT_ASSERT(sin_inet != nullptr);

  CPPUNIT_ASSERT(sin_inet->sin_addr.s_addr == htonl(0x01020304));
  
  torrent::sa_unique_ptr sa_inet6 = wrap_ai_get_first_sa("ff01::1");
  torrent::sa_in6_unique_ptr sin6_inet6;

  CPPUNIT_ASSERT(sa_inet6 != nullptr);
  CPPUNIT_ASSERT_NO_THROW({ sin6_inet6 = torrent::sin6_from_sa(std::move(sa_inet6)); });
  CPPUNIT_ASSERT(sa_inet6 == nullptr);
  CPPUNIT_ASSERT(sin6_inet6 != nullptr);
  
  CPPUNIT_ASSERT(sin6_inet6->sin6_addr.s6_addr[0] == 0xff);
  CPPUNIT_ASSERT(sin6_inet6->sin6_addr.s6_addr[1] == 0x01);
  CPPUNIT_ASSERT(sin6_inet6->sin6_addr.s6_addr[15] == 0x01);

  CPPUNIT_ASSERT_THROW(torrent::sin_from_sa(torrent::sa_unique_ptr()), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sin_from_sa(torrent::sa_make_unspec()), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sin_from_sa(torrent::sa_make_inet6()), torrent::internal_error);

  CPPUNIT_ASSERT_THROW(torrent::sin6_from_sa(torrent::sa_unique_ptr()), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sin6_from_sa(torrent::sa_make_unspec()), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sin6_from_sa(torrent::sa_make_inet()), torrent::internal_error);
}

void
test_socket_address::test_sa_in_compare() {
}
