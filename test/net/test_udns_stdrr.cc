#include "config.h"

#include "test/net/test_udns_stdrr.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "net/udns_library.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_udns_stdrr, "net");

namespace {

const uint8_t guard_byte = 0x7f;
const size_t  guard_size = 2048;
const size_t  packet_size = 400;

// Offset of the question name within the reply, and the offset the question
// name's compression pointer resolves to.
const unsigned question_offset = 12;
const unsigned target_offset = 250;

const uint8_t question_dn[] = { 4, 't', 'e', 's', 't', 0 };

void
put16(std::vector<uint8_t>& packet, unsigned offset, unsigned value) {
  packet[offset] = static_cast<uint8_t>(value >> 8);
  packet[offset + 1] = static_cast<uint8_t>(value & 0xff);
}

// A reply header for one question and 'answers' answers.
std::vector<uint8_t>
reply_header(unsigned answers) {
  std::vector<uint8_t> packet(packet_size, 0x01);

  put16(packet, 0, 0x1234);
  put16(packet, 2, 0x8180);
  put16(packet, 4, 1);
  put16(packet, 6, answers);
  put16(packet, 8, 0);
  put16(packet, 10, 0);

  // The question name is a compression pointer rather than a literal name, so
  // the wire bytes at question_offset decode to "test" but read as a 192 byte
  // label followed by more label data. Every such byte needs four characters in
  // presentation form, which carries the undecompressed name past DNS_MAXNAME.
  packet[question_offset] = 0xC0;
  packet[question_offset + 1] = target_offset;
  put16(packet, 14, DNS_T_A);
  put16(packet, 16, DNS_C_IN);

  packet[target_offset] = 4;
  packet[target_offset + 1] = 't';
  packet[target_offset + 2] = 'e';
  packet[target_offset + 3] = 's';
  packet[target_offset + 4] = 't';
  packet[target_offset + 5] = 0;

  packet[390] = 0;

  return packet;
}

// Run the answer section the way the record parsers in udns_rr_*.c do.
void
parse_answers(struct dns_parse& p, const std::vector<uint8_t>& packet) {
  struct dns_rr rr;

  dns_initparse(&p, question_dn, packet.data(), packet.data() + 14,
                packet.data() + packet.size());

  while (dns_nextrr(&p, &rr) > 0)
    ;
}

// Fill a buffer of exactly the size dns_stdrr_size() asked for, followed by
// untouched guard bytes, and report the first guard byte dns_stdrr_finish()
// overwrote.
size_t
finish_and_find_overrun(const struct dns_parse& p, struct dns_rr_null& result,
                        std::vector<char>& buffer) {
  int size = dns_stdrr_size(&p);

  CPPUNIT_ASSERT(size > 0);

  buffer.assign(size + guard_size, static_cast<char>(guard_byte));

  dns_stdrr_finish(&result, buffer.data(), &p);

  for (size_t i = size; i != buffer.size(); i++)
    if (static_cast<uint8_t>(buffer[i]) != guard_byte)
      return i;

  return buffer.size();
}

} // namespace

// A compressed question name must not make dns_stdrr_finish() write past the
// space dns_stdrr_size() reserved.
void
test_udns_stdrr::test_compressed_question_name() {
  auto packet = reply_header(1);

  // A single A record, its name a pointer to the question name.
  packet[18] = 0xC0;
  packet[19] = question_offset;
  put16(packet, 20, DNS_T_A);
  put16(packet, 22, DNS_C_IN);
  put16(packet, 24, 0);
  put16(packet, 26, 60);
  put16(packet, 28, 4);
  packet[30] = 1;
  packet[31] = 2;
  packet[32] = 3;
  packet[33] = 4;

  struct dns_parse p;
  parse_answers(p, packet);

  CPPUNIT_ASSERT_EQUAL(1, p.dnsp_nrr);

  struct dns_rr_null result;
  std::vector<char>  buffer;
  size_t             buffer_size = dns_stdrr_size(&p) + guard_size;

  CPPUNIT_ASSERT_EQUAL(buffer_size, finish_and_find_overrun(p, result, buffer));

  CPPUNIT_ASSERT(std::strcmp(result.dnsn_cname, "test") == 0);
  CPPUNIT_ASSERT(std::strcmp(result.dnsn_qname, "test") == 0);
}

// The same, for the CNAME path, where the question name and the final name are
// stored as two separate strings.
void
test_udns_stdrr::test_compressed_question_name_with_cname() {
  auto packet = reply_header(2);

  // A CNAME record for the question name, pointing at "alias".
  packet[18] = 0xC0;
  packet[19] = question_offset;
  put16(packet, 20, DNS_T_CNAME);
  put16(packet, 22, DNS_C_IN);
  put16(packet, 24, 0);
  put16(packet, 26, 60);
  put16(packet, 28, 7);
  packet[30] = 5;
  packet[31] = 'a';
  packet[32] = 'l';
  packet[33] = 'i';
  packet[34] = 'a';
  packet[35] = 's';
  packet[36] = 0;

  // An A record for "alias", its name a pointer to the CNAME record data.
  packet[37] = 0xC0;
  packet[38] = 30;
  put16(packet, 39, DNS_T_A);
  put16(packet, 41, DNS_C_IN);
  put16(packet, 43, 0);
  put16(packet, 45, 60);
  put16(packet, 47, 4);
  packet[49] = 1;
  packet[50] = 2;
  packet[51] = 3;
  packet[52] = 4;

  struct dns_parse p;
  parse_answers(p, packet);

  CPPUNIT_ASSERT_EQUAL(1, p.dnsp_nrr);

  struct dns_rr_null result;
  std::vector<char>  buffer;
  size_t             buffer_size = dns_stdrr_size(&p) + guard_size;

  CPPUNIT_ASSERT_EQUAL(buffer_size, finish_and_find_overrun(p, result, buffer));

  CPPUNIT_ASSERT(std::strcmp(result.dnsn_cname, "alias") == 0);
  CPPUNIT_ASSERT(std::strcmp(result.dnsn_qname, "test") == 0);
}
