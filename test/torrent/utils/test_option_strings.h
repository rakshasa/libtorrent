#include "helpers/test_main_thread.h"

class test_option_strings : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_option_strings);
  CPPUNIT_TEST(test_entries);
  CPPUNIT_TEST_SUITE_END();

public:
  void test_entries();
};
