#include "test/helpers/test_main_thread.h"

class test_file_manager : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_file_manager);

  CPPUNIT_TEST(test_evict_closes_requested_count);
  CPPUNIT_TEST(test_evict_uses_least_active_cache);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_evict_closes_requested_count();
  void test_evict_uses_least_active_cache();
};
