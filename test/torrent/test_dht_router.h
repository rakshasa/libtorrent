#include "helpers/test_main_thread.h"

class test_dht_router : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_dht_router);

  CPPUNIT_TEST(test_tracker_count_is_capped);
  CPPUNIT_TEST(test_full_table_evicts_stale_trackers);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_tracker_count_is_capped();
  void test_full_table_evicts_stale_trackers();
};
