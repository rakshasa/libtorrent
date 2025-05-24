#include "test/helpers/test_main_thread.h"

class test_extents : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_extents);

  CPPUNIT_TEST(test_basic);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();
};
