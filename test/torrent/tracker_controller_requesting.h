#include <cppunit/extensions/HelperMacros.h>

#include "tracker_controller_test.h"

class tracker_controller_requesting : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(tracker_controller_requesting);

  CPPUNIT_TEST(test_hammering_basic_success);
  CPPUNIT_TEST(test_hammering_basic_success_long_timeout);
  CPPUNIT_TEST(test_hammering_basic_success_short_timeout);
  CPPUNIT_TEST(test_hammering_basic_failure);
  CPPUNIT_TEST(test_hammering_basic_failure_long_timeout);
  CPPUNIT_TEST(test_hammering_basic_failure_short_timeout);

  CPPUNIT_TEST(test_hammering_multi_success);
  CPPUNIT_TEST(test_hammering_multi_success_long_timeout);
  CPPUNIT_TEST(test_hammering_multi_success_short_timeout);
  CPPUNIT_TEST(test_hammering_multi_failure);
  // CPPUNIT_TEST(test_hammering_multi_failure_long_timeout);
  // CPPUNIT_TEST(test_hammering_multi_failure_short_timeout);

  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_hammering_basic_success();
  void test_hammering_basic_success_long_timeout();
  void test_hammering_basic_success_short_timeout();
  void test_hammering_basic_failure();
  void test_hammering_basic_failure_long_timeout();
  void test_hammering_basic_failure_short_timeout();

  void test_hammering_multi_success();
  void test_hammering_multi_success_long_timeout();
  void test_hammering_multi_success_short_timeout();
  void test_hammering_multi_failure();
  void test_hammering_multi_failure_long_timeout();
  void test_hammering_multi_failure_short_timeout();
};
