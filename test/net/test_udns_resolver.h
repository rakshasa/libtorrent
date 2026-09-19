#ifndef LIBTORRENT_TEST_NET_TEST_UDNS_RESOLVER_H
#define LIBTORRENT_TEST_NET_TEST_UDNS_RESOLVER_H

#include "helpers/test_fixture.h"

class test_udns_resolver : public test_fixture {
  CPPUNIT_TEST_SUITE(test_udns_resolver);

  CPPUNIT_TEST(test_random32_exceeds_microsecond_range);
  CPPUNIT_TEST(test_reply_without_qr_flag_is_rejected);
  CPPUNIT_TEST(test_reply_with_qr_flag_is_accepted);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_random32_exceeds_microsecond_range();
  void test_reply_without_qr_flag_is_rejected();
  void test_reply_with_qr_flag_is_accepted();
};

#endif
