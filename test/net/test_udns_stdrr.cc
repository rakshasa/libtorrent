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

// Offset of the question name within the reply.
const unsigned question_offset = 12;

const uint8_t question_dn[] = { 4, 't', 'e', 's', 't', 0 };

void
put16(std::vector<uint8_t>& packet, unsigned offset, unsigned value) {
  packet[offset] = static_cast<uint8_t>(value >> 8);
  packet[offset + 1] = static_cast<uint8_t>(value & 0xff);
}

// A reply with a literal question name and 'answers' answers.
std::vector<uint8_t>
reply_header(unsigned answers) {
  std::vector<uint8_t> packet(22, 0);

  put16(packet, 0, 0x1234);
  put16(packet, 2, 0x8180);
  put16(packet, 4, 1);
  put16(packet, 6, answers);
  put16(packet, 8, 0);
  put16(packet, 10, 0);

  std::memcpy(packet.data() + question_offset, question_dn, sizeof(question_dn));
  put16(packet, 18, DNS_T_A);
  put16(packet, 20, DNS_C_IN);

  return packet;
}

// Run the answer section the way the record parsers in udns_rr_*.c do.
void
parse_answers(struct dns_parse& p, const std::vector<uint8_t>& packet) {
  struct dns_rr rr;

  dns_initparse(&p, question_dn, packet.data(), packet.data() + 18,
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

// A valid literal question name must not make dns_stdrr_finish() write past
// the space dns_stdrr_size() reserved.
void
test_udns_stdrr::test_valid_question_name() {
  auto packet = reply_header(1);
  packet.resize(38);

  // A single A record, its name a pointer to the question name.
  packet[22] = 0xC0;
  packet[23] = question_offset;
  put16(packet, 24, DNS_T_A);
  put16(packet, 26, DNS_C_IN);
  put16(packet, 28, 0);
  put16(packet, 30, 60);
  put16(packet, 32, 4);
  packet[34] = 1;
  packet[35] = 2;
  packet[36] = 3;
  packet[37] = 4;

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

// The CNAME path stores the question name and the final name as separate
// strings.
void
test_udns_stdrr::test_valid_question_name_with_cname() {
  auto packet = reply_header(2);
  packet.resize(57);

  // A CNAME record for the question name, pointing at "alias".
  packet[22] = 0xC0;
  packet[23] = question_offset;
  put16(packet, 24, DNS_T_CNAME);
  put16(packet, 26, DNS_C_IN);
  put16(packet, 28, 0);
  put16(packet, 30, 60);
  put16(packet, 32, 7);
  packet[34] = 5;
  packet[35] = 'a';
  packet[36] = 'l';
  packet[37] = 'i';
  packet[38] = 'a';
  packet[39] = 's';
  packet[40] = 0;

  // An A record for "alias", its name a pointer to the CNAME record data.
  packet[41] = 0xC0;
  packet[42] = 34;
  put16(packet, 43, DNS_T_A);
  put16(packet, 45, DNS_C_IN);
  put16(packet, 47, 0);
  put16(packet, 49, 60);
  put16(packet, 51, 4);
  packet[53] = 1;
  packet[54] = 2;
  packet[55] = 3;
  packet[56] = 4;

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
