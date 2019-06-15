#include "config.h"

#include "test_socket_address.h"

#include "helpers/network.h"
#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_socket_address, "torrent/net");

static const auto sin_m1 = wrap_ai_get_first_sa("1.2.3.4");
static const auto sin_c1 = wrap_ai_get_first_c_sa("1.2.3.4");
static const auto sin_m2 = wrap_ai_get_first_sa("100.2.3.4");
static const auto sin_c2 = wrap_ai_get_first_c_sa("100.2.3.4");
static const auto sin6_m1 = wrap_ai_get_first_sa("ff01::1");
static const auto sin6_c1 = wrap_ai_get_first_c_sa("ff01::1");
static const auto sin6_m2 = wrap_ai_get_first_sa("ff01::100");
static const auto sin6_c2 = wrap_ai_get_first_c_sa("ff01::100");

static const auto sin_m1_p1 = wrap_ai_get_first_sa("1.2.3.4", "5555");
static const auto sin_m1_p2 = wrap_ai_get_first_sa("1.2.3.4", "7777");
static const auto sin_m2_p1 = wrap_ai_get_first_sa("100.2.3.4", "5555");
static const auto sin_m2_p2 = wrap_ai_get_first_sa("100.2.3.4", "7777");
static const auto sin6_m1_p1 = wrap_ai_get_first_sa("ff01::1", "5555");
static const auto sin6_m1_p2 = wrap_ai_get_first_sa("ff01::1", "7777");
static const auto sin6_m2_p1 = wrap_ai_get_first_sa("ff01::100", "5555");
static const auto sin6_m2_p2 = wrap_ai_get_first_sa("ff01::100", "7777");

void
test_socket_address::test_make() {
  torrent::sa_unique_ptr sa_unspec = torrent::sa_make_unspec();
  CPPUNIT_ASSERT(sa_unspec != nullptr);
  CPPUNIT_ASSERT(sa_unspec->sa_family == AF_UNSPEC);

  torrent::sa_unique_ptr sa_inet = torrent::sa_make_inet();
  CPPUNIT_ASSERT(sa_inet != nullptr);
  CPPUNIT_ASSERT(sa_inet->sa_family == AF_INET);

  sockaddr_in* sin_inet = reinterpret_cast<sockaddr_in*>(sa_inet.get());
  CPPUNIT_ASSERT(sin_inet->sin_family == AF_INET);
  CPPUNIT_ASSERT(sin_inet->sin_port == 0);
  CPPUNIT_ASSERT(sin_inet->sin_addr.s_addr == in_addr().s_addr);

  torrent::sa_unique_ptr sa_inet6 = torrent::sa_make_inet6();
  CPPUNIT_ASSERT(sa_inet6 != nullptr);
  CPPUNIT_ASSERT(sa_inet6->sa_family == AF_INET6);

  sockaddr_in6* sin6_inet6 = reinterpret_cast<sockaddr_in6*>(sa_inet6.get());
  CPPUNIT_ASSERT(sin6_inet6->sin6_family == AF_INET6);
  CPPUNIT_ASSERT(sin6_inet6->sin6_port == 0);
  CPPUNIT_ASSERT(sin6_inet6->sin6_flowinfo == 0);
  CPPUNIT_ASSERT(compare_sin6_addr(sin6_inet6->sin6_addr, in6_addr{0}));
  CPPUNIT_ASSERT(sin6_inet6->sin6_scope_id == 0);

  torrent::sa_unique_ptr sa_unix = torrent::sa_make_unix("");
  CPPUNIT_ASSERT(sa_unix != nullptr);
  CPPUNIT_ASSERT(sa_unix->sa_family == AF_UNIX);
}

void
test_socket_address::test_sin_from_sa() {
  torrent::sa_unique_ptr sa_zero = wrap_ai_get_first_sa("0.0.0.0");
  torrent::sin_unique_ptr sin_zero;

  CPPUNIT_ASSERT(sa_zero != nullptr);
  CPPUNIT_ASSERT_NO_THROW({ sin_zero = torrent::sin_from_sa(std::move(sa_zero)); });
  CPPUNIT_ASSERT(sa_zero == nullptr);
  CPPUNIT_ASSERT(sin_zero != nullptr);

  CPPUNIT_ASSERT(sin_zero->sin_addr.s_addr == htonl(0x0));

  torrent::sa_unique_ptr sa_inet = wrap_ai_get_first_sa("1.2.3.4");
  torrent::sin_unique_ptr sin_inet;

  CPPUNIT_ASSERT(sa_inet != nullptr);
  CPPUNIT_ASSERT_NO_THROW({ sin_inet = torrent::sin_from_sa(std::move(sa_inet)); });
  CPPUNIT_ASSERT(sa_inet == nullptr);
  CPPUNIT_ASSERT(sin_inet != nullptr);

  CPPUNIT_ASSERT(sin_inet->sin_addr.s_addr == htonl(0x01020304));
  
  CPPUNIT_ASSERT_THROW(torrent::sin_from_sa(torrent::sa_unique_ptr()), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sin_from_sa(torrent::sa_make_unspec()), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sin_from_sa(torrent::sa_make_inet6()), torrent::internal_error);
}

void
test_socket_address::test_sin6_from_sa() {
  torrent::sa_unique_ptr sa_zero = wrap_ai_get_first_sa("::");
  torrent::sin6_unique_ptr sin6_zero;

  CPPUNIT_ASSERT(sa_zero != nullptr);
  CPPUNIT_ASSERT_NO_THROW({ sin6_zero = torrent::sin6_from_sa(std::move(sa_zero)); });
  CPPUNIT_ASSERT(sa_zero == nullptr);
  CPPUNIT_ASSERT(sin6_zero != nullptr);

  CPPUNIT_ASSERT(sin6_zero->sin6_addr.s6_addr[0] == 0x0);
  CPPUNIT_ASSERT(sin6_zero->sin6_addr.s6_addr[1] == 0x0);
  CPPUNIT_ASSERT(sin6_zero->sin6_addr.s6_addr[15] == 0x0);

  torrent::sa_unique_ptr sa_inet6 = wrap_ai_get_first_sa("ff01::1");
  torrent::sin6_unique_ptr sin6_inet6;

  CPPUNIT_ASSERT(sa_inet6 != nullptr);
  CPPUNIT_ASSERT_NO_THROW({ sin6_inet6 = torrent::sin6_from_sa(std::move(sa_inet6)); });
  CPPUNIT_ASSERT(sa_inet6 == nullptr);
  CPPUNIT_ASSERT(sin6_inet6 != nullptr);
  
  CPPUNIT_ASSERT(sin6_inet6->sin6_addr.s6_addr[0] == 0xff);
  CPPUNIT_ASSERT(sin6_inet6->sin6_addr.s6_addr[1] == 0x01);
  CPPUNIT_ASSERT(sin6_inet6->sin6_addr.s6_addr[15] == 0x01);

  CPPUNIT_ASSERT_THROW(torrent::sin6_from_sa(torrent::sa_unique_ptr()), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sin6_from_sa(torrent::sa_make_unspec()), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sin6_from_sa(torrent::sa_make_inet()), torrent::internal_error);
}

void
test_socket_address::test_sa_equal() {
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sa_make_unspec(), torrent::sa_make_unspec()));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sa_make_inet(), torrent::sa_make_inet()));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sa_make_inet6(), torrent::sa_make_inet6()));

  CPPUNIT_ASSERT(!torrent::sap_equal(torrent::sa_make_unspec(), torrent::sa_make_inet()));
  CPPUNIT_ASSERT(!torrent::sap_equal(torrent::sa_make_unspec(), torrent::sa_make_inet6()));
  CPPUNIT_ASSERT(!torrent::sap_equal(torrent::sa_make_inet(), torrent::sa_make_inet6()));
  CPPUNIT_ASSERT(!torrent::sap_equal(torrent::sa_make_inet6(), torrent::sa_make_inet()));

  CPPUNIT_ASSERT(torrent::sap_equal(sin_m1, sin_m1));
  CPPUNIT_ASSERT(torrent::sap_equal(sin_m1, sin_c1));
  CPPUNIT_ASSERT(torrent::sap_equal(sin_c1, sin_m1));
  CPPUNIT_ASSERT(torrent::sap_equal(sin_c1, sin_c1));

  CPPUNIT_ASSERT(!torrent::sap_equal(sin_m1, sin_m2));
  CPPUNIT_ASSERT(!torrent::sap_equal(sin_m1, sin_c2));
  CPPUNIT_ASSERT(!torrent::sap_equal(sin_c1, sin_m2));
  CPPUNIT_ASSERT(!torrent::sap_equal(sin_c1, sin_c2));

  CPPUNIT_ASSERT(torrent::sap_equal(sin6_m1, sin6_m1));
  CPPUNIT_ASSERT(torrent::sap_equal(sin6_m1, sin6_c1));
  CPPUNIT_ASSERT(torrent::sap_equal(sin6_c1, sin6_m1));
  CPPUNIT_ASSERT(torrent::sap_equal(sin6_c1, sin6_c1));

  CPPUNIT_ASSERT(!torrent::sap_equal(sin6_m1, sin6_m2));
  CPPUNIT_ASSERT(!torrent::sap_equal(sin6_m1, sin6_c2));
  CPPUNIT_ASSERT(!torrent::sap_equal(sin6_c1, sin6_m2));
  CPPUNIT_ASSERT(!torrent::sap_equal(sin6_c1, sin6_c2));

  CPPUNIT_ASSERT(torrent::sap_equal(sin_m1_p1, sin_m1_p1));
  CPPUNIT_ASSERT(torrent::sap_equal(sin6_m1_p1, sin6_m1_p1));
  CPPUNIT_ASSERT(!torrent::sap_equal(sin_m1_p1, sin_m1_p2));
  CPPUNIT_ASSERT(!torrent::sap_equal(sin6_m1_p1, sin6_m1_p2));
  CPPUNIT_ASSERT(!torrent::sap_equal(sin_m1_p1, sin_m2_p1));
  CPPUNIT_ASSERT(!torrent::sap_equal(sin6_m1_p1, sin6_m2_p1));
  CPPUNIT_ASSERT(!torrent::sap_equal(sin_m1_p1, sin_m2_p2));
  CPPUNIT_ASSERT(!torrent::sap_equal(sin6_m1_p1, sin6_m2_p2));

  CPPUNIT_ASSERT_THROW(torrent::sap_equal(torrent::sa_make_unix(""), torrent::sa_make_unix("")), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sap_equal(torrent::sa_make_unix(""), sin6_m1), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sap_equal(sin6_m1, torrent::sa_make_unix("")), torrent::internal_error);
}

void
test_socket_address::test_sa_equal_addr() {
  CPPUNIT_ASSERT(torrent::sap_equal_addr(torrent::sa_make_unspec(), torrent::sa_make_unspec()));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(torrent::sa_make_inet(), torrent::sa_make_inet()));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(torrent::sa_make_inet6(), torrent::sa_make_inet6()));

  CPPUNIT_ASSERT(!torrent::sap_equal_addr(torrent::sa_make_unspec(), torrent::sa_make_inet()));
  CPPUNIT_ASSERT(!torrent::sap_equal_addr(torrent::sa_make_unspec(), torrent::sa_make_inet6()));
  CPPUNIT_ASSERT(!torrent::sap_equal_addr(torrent::sa_make_inet(), torrent::sa_make_inet6()));
  CPPUNIT_ASSERT(!torrent::sap_equal_addr(torrent::sa_make_inet6(), torrent::sa_make_inet()));

  CPPUNIT_ASSERT(torrent::sap_equal_addr(sin_m1, sin_m1));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sin_m1, sin_c1));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sin_c1, sin_m1));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sin_c1, sin_c1));

  CPPUNIT_ASSERT(!torrent::sap_equal_addr(sin_m1, sin_m2));
  CPPUNIT_ASSERT(!torrent::sap_equal_addr(sin_m1, sin_c2));
  CPPUNIT_ASSERT(!torrent::sap_equal_addr(sin_c1, sin_m2));
  CPPUNIT_ASSERT(!torrent::sap_equal_addr(sin_c1, sin_c2));

  CPPUNIT_ASSERT(torrent::sap_equal_addr(sin6_m1, sin6_m1));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sin6_m1, sin6_c1));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sin6_c1, sin6_m1));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sin6_c1, sin6_c1));

  CPPUNIT_ASSERT(!torrent::sap_equal_addr(sin6_m1, sin6_m2));
  CPPUNIT_ASSERT(!torrent::sap_equal_addr(sin6_m1, sin6_c2));
  CPPUNIT_ASSERT(!torrent::sap_equal_addr(sin6_c1, sin6_m2));
  CPPUNIT_ASSERT(!torrent::sap_equal_addr(sin6_c1, sin6_c2));

  CPPUNIT_ASSERT(torrent::sap_equal_addr(sin_m1_p1, sin_m1_p1));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sin6_m1_p1, sin6_m1_p1));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sin_m1_p1, sin_m1_p2));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sin6_m1_p1, sin6_m1_p2));
  CPPUNIT_ASSERT(!torrent::sap_equal_addr(sin_m1_p1, sin_m2_p1));
  CPPUNIT_ASSERT(!torrent::sap_equal_addr(sin6_m1_p1, sin6_m2_p1));
  CPPUNIT_ASSERT(!torrent::sap_equal_addr(sin_m1_p1, sin_m2_p2));
  CPPUNIT_ASSERT(!torrent::sap_equal_addr(sin6_m1_p1, sin6_m2_p2));

  CPPUNIT_ASSERT_THROW(torrent::sap_equal_addr(torrent::sa_make_unix(""), torrent::sa_make_unix("")), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sap_equal_addr(torrent::sa_make_unix(""), sin6_m1), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sap_equal_addr(sin6_m1, torrent::sa_make_unix("")), torrent::internal_error);
}

void
test_socket_address::test_sa_copy() {
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(torrent::sa_unique_ptr()), torrent::sa_make_unspec()));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(torrent::c_sa_unique_ptr()), torrent::sa_make_unspec()));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(torrent::sa_make_unspec()), torrent::sa_make_unspec()));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(torrent::sa_make_inet()), torrent::sa_make_inet()));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(torrent::sa_make_inet6()), torrent::sa_make_inet6()));

  CPPUNIT_ASSERT(torrent::sap_copy(sin_m1).get() != sin_m1.get());
  CPPUNIT_ASSERT(torrent::sap_copy(sin_c1).get() != sin_c1.get());
  CPPUNIT_ASSERT(torrent::sap_copy(sin6_m1).get() != sin6_m1.get());
  CPPUNIT_ASSERT(torrent::sap_copy(sin6_c1).get() != sin6_c1.get());
  CPPUNIT_ASSERT(torrent::sap_copy(sin_m1_p1).get() != sin_m1_p1.get());
  CPPUNIT_ASSERT(torrent::sap_copy(sin6_m1_p1).get() != sin6_m1_p1.get());

  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(sin_m1), sin_m1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(sin_m1), sin_c1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(sin_c1), sin_m1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(sin_c1), sin_c1));

  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(sin6_m1), sin6_m1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(sin6_m1), sin6_c1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(sin6_c1), sin6_m1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(sin6_c1), sin6_c1));

  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(sin_m1_p1), sin_m1_p1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(sin6_m1_p1), sin6_m1_p1));

  auto sin6_flags = wrap_ai_get_first_sa("ff01::1", "5555");
  reinterpret_cast<sockaddr_in6*>(sin6_flags.get())->sin6_flowinfo = 0x12345678;
  reinterpret_cast<sockaddr_in6*>(sin6_flags.get())->sin6_scope_id = 0x12345678;

  // TODO: Need 'strict' equal test.
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy(sin6_flags), sin6_flags));
}

void
test_socket_address::test_sa_copy_addr() {
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(torrent::sa_unique_ptr()), torrent::sa_make_unspec()));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(torrent::c_sa_unique_ptr()), torrent::sa_make_unspec()));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(torrent::sa_make_unspec()), torrent::sa_make_unspec()));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(torrent::sa_make_inet()), torrent::sa_make_inet()));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(torrent::sa_make_inet6()), torrent::sa_make_inet6()));

  CPPUNIT_ASSERT(torrent::sap_copy_addr(sin_m1).get() != sin_m1.get());
  CPPUNIT_ASSERT(torrent::sap_copy_addr(sin_c1).get() != sin_c1.get());
  CPPUNIT_ASSERT(torrent::sap_copy_addr(sin6_m1).get() != sin6_m1.get());
  CPPUNIT_ASSERT(torrent::sap_copy_addr(sin6_c1).get() != sin6_c1.get());
  CPPUNIT_ASSERT(torrent::sap_copy_addr(sin_m1_p1).get() != sin_m1_p1.get());
  CPPUNIT_ASSERT(torrent::sap_copy_addr(sin6_m1_p1).get() != sin6_m1_p1.get());

  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(sin_m1), sin_m1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(sin_m1), sin_c1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(sin_c1), sin_m1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(sin_c1), sin_c1));

  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(sin6_m1), sin6_m1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(sin6_m1), sin6_c1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(sin6_c1), sin6_m1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(sin6_c1), sin6_c1));

  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(sin_m1_p1), sin_m1));
  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(sin6_m1_p1), sin6_m1));

  auto sin6_flags = wrap_ai_get_first_sa("ff01::1", "5555");
  reinterpret_cast<sockaddr_in6*>(sin6_flags.get())->sin6_flowinfo = 0x12345678;
  reinterpret_cast<sockaddr_in6*>(sin6_flags.get())->sin6_scope_id = 0x12345678;

  CPPUNIT_ASSERT(torrent::sap_equal(torrent::sap_copy_addr(sin6_flags), sin6_m1));
}

void
test_socket_address::test_sa_is_broadcast() {
  TEST_DEFAULT_SA;

  CPPUNIT_ASSERT(torrent::sap_is_broadcast(sin_broadcast));

  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(sin_any));
  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(sin_1));
  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(sin6_any));
  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(sin6_1));

  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(c_sin_any));
  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(c_sin_1));
  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(c_sin6_any));
  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(c_sin6_1));

  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(sin_any_5050));
  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(sin_1_5050));
  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(sin6_any_5050));
  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(sin6_1_5050));

  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(c_sin_any_5050));
  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(c_sin_1_5050));
  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(c_sin6_any_5050));
  CPPUNIT_ASSERT(!torrent::sap_is_broadcast(c_sin6_1_5050));
}

void
test_socket_address::test_sa_from_v4mapped() {
  torrent::sa_unique_ptr sa_zero = torrent::sap_from_v4mapped(wrap_ai_get_first_sa("::ffff:0.0.0.0"));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sa_zero, wrap_ai_get_first_sa("0.0.0.0")));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(sa_zero));

  torrent::sa_unique_ptr sa_1 = torrent::sap_from_v4mapped(wrap_ai_get_first_sa("::ffff:1.2.3.4"));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sa_1, wrap_ai_get_first_sa("1.2.3.4")));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(sa_1));

  torrent::sa_unique_ptr sa_bc = torrent::sap_from_v4mapped(wrap_ai_get_first_sa("::ffff:255.255.255.255"));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sa_bc, wrap_ai_get_first_sa("255.255.255.255")));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(sa_bc));

  CPPUNIT_ASSERT_THROW(torrent::sap_from_v4mapped(torrent::sa_make_unspec()), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sap_from_v4mapped(torrent::sa_make_inet()), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sap_from_v4mapped(torrent::sa_make_unix("")), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sap_from_v4mapped(wrap_ai_get_first_sa("1.2.3.4")), torrent::internal_error);
}

void
test_socket_address::test_sa_to_v4mapped() {
  torrent::sa_unique_ptr sa_zero = torrent::sap_to_v4mapped(wrap_ai_get_first_sa("0.0.0.0"));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sa_zero, wrap_ai_get_first_sa("::ffff:0.0.0.0")));
  CPPUNIT_ASSERT(torrent::sap_is_v4mapped(sa_zero));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(sa_zero));

  torrent::sa_unique_ptr sa_1 = torrent::sap_to_v4mapped(wrap_ai_get_first_sa("1.2.3.4"));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sa_1, wrap_ai_get_first_sa("::ffff:1.2.3.4")));
  CPPUNIT_ASSERT(torrent::sap_is_v4mapped(sa_1));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(sa_1));

  torrent::sa_unique_ptr sa_bc = torrent::sap_to_v4mapped(wrap_ai_get_first_sa("255.255.255.255"));
  CPPUNIT_ASSERT(torrent::sap_equal_addr(sa_bc, wrap_ai_get_first_sa("::ffff:255.255.255.255")));
  CPPUNIT_ASSERT(torrent::sap_is_v4mapped(sa_bc));
  CPPUNIT_ASSERT(torrent::sap_is_port_any(sa_bc));

  CPPUNIT_ASSERT_THROW(torrent::sap_to_v4mapped(torrent::sa_make_unspec()), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sap_to_v4mapped(torrent::sa_make_inet6()), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sap_to_v4mapped(torrent::sa_make_unix("")), torrent::internal_error);
  CPPUNIT_ASSERT_THROW(torrent::sap_to_v4mapped(wrap_ai_get_first_sa("::ffff:1.2.3.4")), torrent::internal_error);
}
