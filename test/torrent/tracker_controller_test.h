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
  CPPUNIT_TEST(test_send_update_normal);
  CPPUNIT_TEST(test_send_task_timeout);
  CPPUNIT_TEST(test_send_close_on_enable);

  CPPUNIT_TEST(test_multiple_success);
  CPPUNIT_TEST(test_multiple_failure);
  CPPUNIT_TEST(test_multiple_cycle);
  CPPUNIT_TEST(test_multiple_cycle_second_group);
  CPPUNIT_TEST(test_multiple_send_stop);

  CPPUNIT_TEST(test_requesting_basic);
  CPPUNIT_TEST(test_requesting_timeout);
  CPPUNIT_TEST(test_multiple_promiscious_timeout);
  CPPUNIT_TEST(test_multiple_promiscious_failed);

  CPPUNIT_TEST(test_timeout_lacking_usable);
  CPPUNIT_TEST(test_disable_tracker);
  CPPUNIT_TEST(test_new_peers);

  CPPUNIT_TEST(test_scrape_basic);
  CPPUNIT_TEST(test_scrape_priority);

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
  void test_send_update_normal();
  void test_send_task_timeout();
  void test_send_close_on_enable();

  void test_multiple_success();
  void test_multiple_failure();
  void test_multiple_cycle();
  void test_multiple_cycle_second_group();
  void test_multiple_send_stop();
  void test_multiple_send_update();

  void test_requesting_basic();
  void test_requesting_timeout();
  void test_multiple_promiscious_timeout();
  void test_multiple_promiscious_failed();

  void test_timeout_lacking_usable();
  void test_disable_tracker();
  void test_new_peers();

  void test_scrape_basic();
  void test_scrape_priority();
};
