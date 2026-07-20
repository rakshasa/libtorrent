#include "helpers/test_main_thread.h"

class test_log_buffer : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_log_buffer);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_timestamps);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();
  void test_timestamps();
};
