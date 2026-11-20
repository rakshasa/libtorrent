#include "config.h"

#include "test_socket_address_key.h"

#include <type_traits>

#include "helpers/network.h"
#include "torrent/net/socket_address.h"
#include "torrent/net/socket_address_key.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_socket_address_key, "torrent/net");

static_assert(alignof(torrent::socket_address_key) >= alignof(uint32_t));
static_assert(std::is_trivially_copyable_v<torrent::socket_address_key>);

void
test_socket_address_key::test_basic() {
  TEST_DEFAULT_SA;

  torrent::socket_address_key default_key;
  CPPUNIT_ASSERT(!default_key.is_valid());

  auto null_key = torrent::socket_address_key::from_sockaddr(nullptr);
  CPPUNIT_ASSERT(!null_key.is_valid());
  CPPUNIT_ASSERT(default_key == null_key);

  auto unspec_sa = torrent::sa_make_unspec();
  auto unix_sa   = torrent::sa_make_unix("");
  CPPUNIT_ASSERT(!torrent::socket_address_key::is_comparable_sockaddr(nullptr));
  CPPUNIT_ASSERT(!torrent::socket_address_key::is_comparable_sockaddr(unspec_sa.get()));
  CPPUNIT_ASSERT(!torrent::socket_address_key::is_comparable_sockaddr(unix_sa.get()));
  CPPUNIT_ASSERT(!torrent::socket_address_key::from_sockaddr(unspec_sa.get()).is_valid());
  CPPUNIT_ASSERT(!torrent::socket_address_key::from_sockaddr(unix_sa.get()).is_valid());

  CPPUNIT_ASSERT(torrent::socket_address_key::is_comparable_sockaddr(sin_1.get()));
  CPPUNIT_ASSERT(torrent::socket_address_key::is_comparable_sockaddr(sin6_1.get()));

  auto key_v4      = torrent::socket_address_key::from_sockaddr(sin_1.get());
  auto key_v4_port = torrent::socket_address_key::from_sockaddr(sin_1_5000.get());
  auto key_v4_sin  = torrent::socket_address_key::from_sin_addr(*reinterpret_cast<const sockaddr_in*>(sin_1.get()));

  CPPUNIT_ASSERT(key_v4.is_valid());
  CPPUNIT_ASSERT(key_v4 == key_v4_port);
  CPPUNIT_ASSERT(key_v4 == key_v4_sin);

  auto key_v6      = torrent::socket_address_key::from_sockaddr(sin6_1.get());
  auto key_v6_port = torrent::socket_address_key::from_sockaddr(sin6_1_5000.get());
  auto key_v6_sin6 = torrent::socket_address_key::from_sin6_addr(*reinterpret_cast<const sockaddr_in6*>(sin6_1.get()));

  CPPUNIT_ASSERT(key_v6.is_valid());
  CPPUNIT_ASSERT(key_v6 == key_v6_port);
  CPPUNIT_ASSERT(key_v6 == key_v6_sin6);

  CPPUNIT_ASSERT(!(key_v4 == key_v6));
}

void
test_socket_address_key::test_ordering() {
  torrent::socket_address_key invalid;

  auto v4_low  = torrent::socket_address_key::from_sockaddr(wrap_ai_get_first_sa("1.0.0.255").get());
  auto v4_high = torrent::socket_address_key::from_sockaddr(wrap_ai_get_first_sa("2.0.0.1").get());

  auto v6_low  = torrent::socket_address_key::from_sockaddr(wrap_ai_get_first_sa("::1").get());
  auto v6_mid  = torrent::socket_address_key::from_sockaddr(wrap_ai_get_first_sa("2001:db8::1").get());
  auto v6_high = torrent::socket_address_key::from_sockaddr(wrap_ai_get_first_sa("ff01::1").get());

  CPPUNIT_ASSERT(invalid < v4_low);
  CPPUNIT_ASSERT(v4_low > invalid);
  CPPUNIT_ASSERT(invalid < v6_low);

  CPPUNIT_ASSERT(v4_low < v4_high);
  CPPUNIT_ASSERT(v4_high > v4_low);
  CPPUNIT_ASSERT(!(v4_low < v4_low));
  CPPUNIT_ASSERT(!(v4_low > v4_low));

  CPPUNIT_ASSERT(v4_high < v6_low);
  CPPUNIT_ASSERT(v6_low > v4_high);

  CPPUNIT_ASSERT(v6_low < v6_mid);
  CPPUNIT_ASSERT(v6_mid < v6_high);
  CPPUNIT_ASSERT(v6_high > v6_low);
  CPPUNIT_ASSERT(!(v6_mid < v6_mid));
  CPPUNIT_ASSERT(!(v6_mid > v6_mid));
}
