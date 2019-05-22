#include "config.h"

#include "test_address_info.h"

#include "helpers/network.h"
#include "torrent/net/address_info.h"
#include "torrent/net/socket_address.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_address_info, "torrent/net");

void
test_address_info::test_basic() {
  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet|aif_any> (std::bind(torrent::ai_get_addrinfo, "0.0.0.0", nullptr, nullptr, std::placeholders::_1)));
  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet6|aif_any>(std::bind(torrent::ai_get_addrinfo, "::", nullptr, nullptr, std::placeholders::_1)));

  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet> (std::bind(torrent::ai_get_addrinfo, "1.1.1.1", nullptr, nullptr, std::placeholders::_1)));
  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet6>(std::bind(torrent::ai_get_addrinfo, "ff01::1", nullptr, nullptr, std::placeholders::_1)));
  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet6>(std::bind(torrent::ai_get_addrinfo, "2001:0db8:85a3:0000:0000:8a2e:0370:7334", nullptr, nullptr, std::placeholders::_1)));

  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet> (std::bind(torrent::ai_get_addrinfo, "1.1.1.1", "22123", nullptr, std::placeholders::_1), 22123));
  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet6>(std::bind(torrent::ai_get_addrinfo, "2001:db8:a::", "22123", nullptr, std::placeholders::_1), 22123));

  CPPUNIT_ASSERT(test_valid_ai_ref<aif_none> (std::bind(torrent::ai_get_addrinfo, "localhost", nullptr, nullptr, std::placeholders::_1)));

  CPPUNIT_ASSERT(test_valid_ai_ref_err(std::bind(torrent::ai_get_addrinfo, "1.1.1.300", nullptr, nullptr, std::placeholders::_1), EAI_NONAME));
  CPPUNIT_ASSERT(test_valid_ai_ref_err(std::bind(torrent::ai_get_addrinfo, "2001:db8:a::22123", nullptr, nullptr, std::placeholders::_1), EAI_NONAME));
}

void
test_address_info::test_numericserv() {
  CPPUNIT_ASSERT(test_valid_ai_ref<aif_inet> (std::bind(torrent::ai_get_addrinfo, "1.1.1.1", nullptr, torrent::ai_make_hint(AI_NUMERICHOST, 0, 0).get(), std::placeholders::_1)));

  CPPUNIT_ASSERT(test_valid_ai_ref_err(std::bind(torrent::ai_get_addrinfo, "localhost", nullptr, torrent::ai_make_hint(AI_NUMERICHOST, 0, 0).get(), std::placeholders::_1), EAI_NONAME));
}

void
test_address_info::test_helpers() {
  torrent::sin_unique_ptr sin_zero = torrent::sin_from_sa(wrap_ai_get_first_sa("0.0.0.0"));
  CPPUNIT_ASSERT(sin_zero != nullptr);
  CPPUNIT_ASSERT(sin_zero->sin_family == AF_INET);
  CPPUNIT_ASSERT(sin_zero->sin_port == 0);
  CPPUNIT_ASSERT(sin_zero->sin_addr.s_addr == in_addr().s_addr);

  torrent::sin_unique_ptr sin_1 = torrent::sin_from_sa(wrap_ai_get_first_sa("1.2.3.4"));
  CPPUNIT_ASSERT(sin_1 != nullptr);
  CPPUNIT_ASSERT(sin_1->sin_family == AF_INET);
  CPPUNIT_ASSERT(sin_1->sin_port == 0);
  CPPUNIT_ASSERT(sin_1->sin_addr.s_addr == htonl(0x01020304));

  torrent::sin6_unique_ptr sin6_zero = torrent::sin6_from_sa(wrap_ai_get_first_sa("::"));
  CPPUNIT_ASSERT(sin6_zero != nullptr);
  CPPUNIT_ASSERT(sin6_zero->sin6_family == AF_INET6);
  CPPUNIT_ASSERT(sin6_zero->sin6_port == 0);
  CPPUNIT_ASSERT(compare_sin6_addr(sin6_zero->sin6_addr, in6_addr{0}));

  torrent::sin6_unique_ptr sin6_1 = torrent::sin6_from_sa(wrap_ai_get_first_sa("ff01::1"));
  CPPUNIT_ASSERT(sin6_1 != nullptr);
  CPPUNIT_ASSERT(sin6_1->sin6_family == AF_INET6);
  CPPUNIT_ASSERT(sin6_1->sin6_port == 0);
  CPPUNIT_ASSERT(compare_sin6_addr(sin6_1->sin6_addr, in6_addr{0xff, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}));
  CPPUNIT_ASSERT(!compare_sin6_addr(sin6_1->sin6_addr, in6_addr{0xff, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2}));
}
