#include "test/helpers/test_main_thread.h"

class TestDhtBucket : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(TestDhtBucket);

  CPPUNIT_TEST(test_empty_bucket_has_no_candidate);
  CPPUNIT_TEST(test_oldest_node_is_the_candidate);
  CPPUNIT_TEST(test_full_bucket_of_max_last_seen_nodes_has_a_candidate);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_empty_bucket_has_no_candidate();
  void test_oldest_node_is_the_candidate();
  void test_full_bucket_of_max_last_seen_nodes_has_a_candidate();
};
