#ifndef LIBTORRENT_TEST_NET_TEST_UDNS_STDRR_H
#define LIBTORRENT_TEST_NET_TEST_UDNS_STDRR_H

#include "helpers/test_fixture.h"

class test_udns_stdrr : public test_fixture {
  CPPUNIT_TEST_SUITE(test_udns_stdrr);

  CPPUNIT_TEST(test_compressed_question_name);
  CPPUNIT_TEST(test_compressed_question_name_with_cname);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_compressed_question_name();
  void test_compressed_question_name_with_cname();
};

#endif
