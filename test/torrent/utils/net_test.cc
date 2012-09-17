#include "config.h"

#include <torrent/exceptions.h>
#include <torrent/utils/net.h>

#include "net_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(utils_net_test);

namespace tr1 { using namespace std::tr1; }

static void inc_value(int* value) { (*value)++; }

#define LTUNIT_AI_CALL(lt_ai, lt_flags) {                               \
  int test_value = 0;                                                   \
  CPPUNIT_ASSERT(torrent::address_info_call(ai, 0, tr1::bind(&inc_value, &test_value))); \
  CPPUNIT_ASSERT(test_value); }                                         \

void
utils_net_test::setUp() {
}

void
utils_net_test::tearDown() {
}

void
utils_net_test::test_basic() {
  addrinfo* ai = torrent::address_info_lookup("localhost", AF_INET, SOCK_STREAM);

  LTUNIT_AI_CALL(ai, 0);

  torrent::address_info_free(ai);
}
