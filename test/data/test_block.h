#include "test/helpers/test_main_thread.h"

class test_block : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_block);

  CPPUNIT_TEST(test_completed_skips_erased_not_stalled_accounting);
  CPPUNIT_TEST(test_invalidate_transfer_without_connection);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_completed_skips_erased_not_stalled_accounting();
  void test_invalidate_transfer_without_connection();
};
