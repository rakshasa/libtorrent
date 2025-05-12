#include "test/helpers/test_fixture.h"
#include "torrent/tracker_controller.h"

class test_tracker_timeout : public test_fixture {
  CPPUNIT_TEST_SUITE(test_tracker_timeout);
  CPPUNIT_TEST(test_set_timeout);

  CPPUNIT_TEST(test_timeout_tracker);
  CPPUNIT_TEST(test_timeout_update);
  CPPUNIT_TEST(test_timeout_requesting);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();

  static void test_set_timeout();

  static void test_timeout_tracker();
  static void test_timeout_update();
  static void test_timeout_requesting();
};
