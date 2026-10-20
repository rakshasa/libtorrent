#include "helpers/test_main_thread.h"

class test_fd : public TestFixtureWithMockAndMainThread {
  CPPUNIT_TEST_SUITE(test_fd);

  CPPUNIT_TEST(test_valid_flags);
  CPPUNIT_TEST(test_open_family_inet6);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_valid_flags();
  void test_open_family_inet6();
};
