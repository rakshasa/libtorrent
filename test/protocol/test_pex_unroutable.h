#include "test/helpers/test_main_thread.h"

class TestPexUnroutable : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(TestPexUnroutable);

  CPPUNIT_TEST(test_ut_pex_skips_unroutable_added6);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_ut_pex_skips_unroutable_added6();
};
