#include "helpers/test_fixture.h"

class test_log_buffer : public test_fixture {
  CPPUNIT_TEST_SUITE(test_log_buffer);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_timestamps);
  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();
  void test_timestamps();
};
