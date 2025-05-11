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

  static void test_basic();

  static void test_single_request();
  static void test_single_canceled();

  static void test_choke_normal();
  static void test_choke_unchoke_discard();
  static void test_choke_unchoke_transfer();
};
