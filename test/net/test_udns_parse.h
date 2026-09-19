#ifndef LIBTORRENT_TEST_NET_TEST_UDNS_PARSE_H
#define LIBTORRENT_TEST_NET_TEST_UDNS_PARSE_H

#include "helpers/test_fixture.h"

class test_udns_parse : public test_fixture {
  CPPUNIT_TEST_SUITE(test_udns_parse);

  CPPUNIT_TEST(test_getdn_max_length_name);
  CPPUNIT_TEST(test_getdn_oversized_name);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_getdn_max_length_name();
  void test_getdn_oversized_name();
};

#endif
