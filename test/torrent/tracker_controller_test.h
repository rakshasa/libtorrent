#include <cppunit/extensions/HelperMacros.h>

#include "torrent/tracker_controller.h"

class tracker_controller_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(tracker_controller_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_enable);
  CPPUNIT_TEST(test_requesting);
  CPPUNIT_TEST(test_timeout);

  CPPUNIT_TEST(test_single_success);
  CPPUNIT_TEST(test_single_failure);
  CPPUNIT_TEST(test_single_disable);

  CPPUNIT_TEST(test_send_start);
  CPPUNIT_TEST(test_send_stop_normal);
  CPPUNIT_TEST(test_send_completed_normal);

  CPPUNIT_TEST(test_multiple_success);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
  void test_enable();
  void test_disable();
  void test_requesting();
  void test_timeout();

  void test_single_success();
  void test_single_failure();
  void test_single_disable();

  void test_send_start();
  void test_send_stop_normal();
  void test_send_completed_normal();

  void test_multiple_success();

  //  void test_single_promiscuous();
};
