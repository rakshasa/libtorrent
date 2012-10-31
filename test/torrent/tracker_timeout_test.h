#include <cppunit/extensions/HelperMacros.h>

#include "torrent/tracker_controller.h"

class tracker_timeout_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(tracker_timeout_test);
  CPPUNIT_TEST(test_set_timeout);

  CPPUNIT_TEST(test_timeout_tracker);
  CPPUNIT_TEST(test_timeout_update);
  CPPUNIT_TEST(test_timeout_requesting);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_set_timeout();

  void test_timeout_tracker();
  void test_timeout_update();
  void test_timeout_requesting();
};
