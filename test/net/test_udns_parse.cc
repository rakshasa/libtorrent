#include "config.h"

#include "test/net/test_udns_parse.h"

#include <cstdint>
#include <vector>

#include "net/udns_library.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_udns_parse, "net");

namespace {

std::vector<uint8_t>
encode_name(const std::vector<unsigned>& label_lengths) {
  std::vector<uint8_t> packet(DNS_HSIZE, 0);

  for (auto length : label_lengths) {
    packet.push_back(static_cast<uint8_t>(length));
    packet.insert(packet.end(), length, 'a');
  }

  packet.push_back(0);

  return packet;
}

int
decode_name(const std::vector<uint8_t>& packet, std::vector<uint8_t>& name) {
  auto cur = packet.data() + DNS_HSIZE;

  return dns_getdn(packet.data(), &cur, packet.data() + packet.size(), name.data(), name.size());
}

} // namespace

void
test_udns_parse::test_getdn_max_length_name() {
  // Four length bytes, 63 + 63 + 63 + 61 label bytes and the root label encode
  // to exactly DNS_MAXDN bytes.
  auto                 packet = encode_name({63, 63, 63, 61});
  std::vector<uint8_t> name(DNS_MAXDN, 0xff);

  CPPUNIT_ASSERT_EQUAL(static_cast<int>(DNS_MAXDN), decode_name(packet, name));
  CPPUNIT_ASSERT_EQUAL(static_cast<int>(0), static_cast<int>(name[DNS_MAXDN - 1]));
}

void
test_udns_parse::test_getdn_oversized_name() {
  // One byte longer than the destination buffer can hold once the root label is
  // written.
  auto                 packet = encode_name({63, 63, 63, 62});
  std::vector<uint8_t> name(DNS_MAXDN, 0xff);

  CPPUNIT_ASSERT_EQUAL(-1, decode_name(packet, name));
}
