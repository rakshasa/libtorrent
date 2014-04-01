#include <cppunit/extensions/HelperMacros.h>

#include "protocol/request_list.h"

class TestRequestList : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(TestRequestList);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_single_request);
  CPPUNIT_TEST(test_single_canceled);

  CPPUNIT_TEST(test_choke_normal);
  CPPUNIT_TEST(test_choke_unchoke_discard);
  CPPUNIT_TEST(test_choke_unchoke_transfer);

  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();

  void test_single_request();
  void test_single_canceled();

  void test_choke_normal();
  void test_choke_unchoke_discard();
  void test_choke_unchoke_transfer();
};
