#include "test/helpers/test_main_thread.h"

class TestTrackerTimeout : public TestFixtureWithMainAndTrackerThread {
  CPPUNIT_TEST_SUITE(TestTrackerTimeout);

  CPPUNIT_TEST(test_set_timeout);

  CPPUNIT_TEST(test_timeout_tracker);
  CPPUNIT_TEST(test_timeout_update);
  CPPUNIT_TEST(test_timeout_requesting);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_set_timeout();

  void test_timeout_tracker();
  void test_timeout_update();
  void test_timeout_requesting();
};
