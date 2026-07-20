#include "test/helpers/test_main_thread.h"
#include "test/torrent/test_tracker_controller.h"

class TestTrackerControllerRequesting : public TestFixtureWithMainAndTrackerThread {
  CPPUNIT_TEST_SUITE(TestTrackerControllerRequesting);

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

  void do_test_hammering_basic(bool success1, bool success2, bool success3, uint32_t min_interval = 0);
  void do_test_hammering_multi3(bool success1, bool success2, bool success3, uint32_t min_interval = 0);
};
