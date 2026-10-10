#include "config.h"

#include "test/protocol/test_pex_added6.h"

#include <arpa/inet.h>
#include <cstring>

#include "net/address_list.h"
#include "torrent/object_static_map.h"
#include "torrent/object_stream.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestPexAdded6);

namespace {

const unsigned char seed_addr[16] = { 0x2a, 0x01, 0x0e, 0x34, 0xec, 0x02, 0xe8, 0x60, 0x9c, 0x66, 0x1f, 0x03, 0xbd, 0xcb, 0x64, 0x28 };

std::string
pex_message() {
  std::string added6(reinterpret_cast<const char*>(seed_addr), 16);
  added6 += std::string("\x8b\x92", 2);

  std::string msg = "d5:added6:";
  msg += std::string("\x01\x02\x03\x04\x1a\xe1", 6);
  msg += "6:added618:" + added6;
  msg += "7:dropped0:e";
  return msg;
}

} // namespace

void
TestPexAdded6::test_pex_added6_parses_to_inet6_addresses() {
  auto msg = pex_message();

  // the "added6" value of the message above: 18 bytes an IPv6 peer
  auto at = msg.find("6:added618:") + std::strlen("6:added618:");

  torrent::AddressList list;
  list.parse_address_compact_ipv6(msg.substr(at, 18));

  CPPUNIT_ASSERT_EQUAL(size_t{1}, list.size());
  CPPUNIT_ASSERT_EQUAL(static_cast<int>(AF_INET6), static_cast<int>(list[0].inet6.sin6_family));
  CPPUNIT_ASSERT(std::memcmp(&list[0].inet6.sin6_addr, seed_addr, 16) == 0);
  CPPUNIT_ASSERT_EQUAL(uint16_t{35730}, ntohs(list[0].inet6.sin6_port));
}
