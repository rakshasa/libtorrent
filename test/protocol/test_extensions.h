#include "test/helpers/test_main_thread.h"

class TestExtensions : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(TestExtensions);

  CPPUNIT_TEST(test_pex_count_released_on_destroy);
  CPPUNIT_TEST(test_pex_count_not_released_twice);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_pex_count_released_on_destroy();
  void test_pex_count_not_released_twice();
};
