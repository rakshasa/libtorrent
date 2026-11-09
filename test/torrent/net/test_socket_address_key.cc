#include "config.h"

#include "test/torrent/net/test_socket_address_key.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <cstring>
#include <cstdint>

#include "torrent/net/socket_address_key.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_socket_address_key, "torrent/net");

void
test_socket_address_key::test_natural_alignment() {
  // The fix removes [[gnu::packed]] so the union member (in6_addr, 4-byte
  // natural alignment) is no longer forced to 1-byte alignment.
  CPPUNIT_ASSERT_EQUAL(alignof(uint32_t), alignof(torrent::socket_address_key));
}

namespace {
struct sockaddr_in make_sin4(uint32_t host_order_addr) {
  struct sockaddr_in sin4;
  std::memset(&sin4, 0, sizeof(sin4));
  sin4.sin_family = AF_INET;
  sin4.sin_addr.s_addr = htonl(host_order_addr);
  return sin4;
}
} // namespace

void
test_socket_address_key::test_ipv4_construction_and_compare() {
  auto sin4  = make_sin4(0x7f000001); // 127.0.0.1
  auto sin4b = make_sin4(0x7f000002); // 127.0.0.2

  torrent::socket_address_key key4  = torrent::socket_address_key::from_sin_addr(sin4);
  torrent::socket_address_key key4b = torrent::socket_address_key::from_sin_addr(sin4b);

  CPPUNIT_ASSERT(key4.is_valid());

  // Self-comparison: equal, not less/greater.
  torrent::socket_address_key key4_from_sa = torrent::socket_address_key::from_sockaddr(
      reinterpret_cast<const sockaddr*>(&sin4));
  CPPUNIT_ASSERT(!(key4 < key4_from_sa));
  CPPUNIT_ASSERT(!(key4 > key4_from_sa));
  CPPUNIT_ASSERT(key4 == key4_from_sa);

  // Lexical ordering: 127.0.0.1 < 127.0.0.2.
  CPPUNIT_ASSERT(key4 < key4b);
  CPPUNIT_ASSERT(key4b > key4);
  CPPUNIT_ASSERT(!(key4 == key4b));
}

void
test_socket_address_key::test_ipv6_construction_and_compare() {
  struct sockaddr_in6 sin6;
  std::memset(&sin6, 0, sizeof(sin6));
  sin6.sin6_family = AF_INET6;
  sin6.sin6_addr = in6addr_loopback; // ::1

  struct sockaddr_in6 sin6b;
  std::memset(&sin6b, 0, sizeof(sin6b));
  sin6b.sin6_family = AF_INET6;
  sin6b.sin6_addr = in6addr_loopback;
  sin6b.sin6_addr.s6_addr[15] = 2; // ::2 (portable: s6_addr32 is glibc-only)

  torrent::socket_address_key key6  = torrent::socket_address_key::from_sin6_addr(sin6);
  torrent::socket_address_key key6b = torrent::socket_address_key::from_sin6_addr(sin6b);

  CPPUNIT_ASSERT(key6.is_valid());

  torrent::socket_address_key key6_from_sa = torrent::socket_address_key::from_sockaddr(
      reinterpret_cast<const sockaddr*>(&sin6));
  CPPUNIT_ASSERT(key6 == key6_from_sa);

  // IPv6 ::1 < ::2.
  CPPUNIT_ASSERT(key6 < key6b);
  CPPUNIT_ASSERT(key6b > key6);
}

void
test_socket_address_key::test_null_sockaddr() {
  torrent::socket_address_key key = torrent::socket_address_key::from_sockaddr(nullptr);
  CPPUNIT_ASSERT(!key.is_valid());
}

void
test_socket_address_key::test_padding_zero_initialized() {
  // from_* zero-initialise the whole struct (result{}), so any padding bytes
  // left by removing [[gnu::packed]] are zero. memcmp-based comparisons and
  // == therefore remain well-defined across the padding: a byte-for-byte
  // copy compares equal to the original.
  auto sin4 = make_sin4(0x7f000001);
  torrent::socket_address_key key = torrent::socket_address_key::from_sin_addr(sin4);

  torrent::socket_address_key key_copy{};
  std::memcpy(&key_copy, &key, sizeof(key));

  CPPUNIT_ASSERT(key == key_copy);
  CPPUNIT_ASSERT(std::memcmp(&key, &key_copy, sizeof(key)) == 0);
}
