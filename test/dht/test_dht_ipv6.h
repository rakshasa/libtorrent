#include "test/helpers/test_main_thread.h"

class TestDhtIpv6 : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(TestDhtIpv6);

  CPPUNIT_TEST(test_node_compact_is_38_bytes);
  CPPUNIT_TEST(test_node_compact_v4_is_still_26_bytes);
  CPPUNIT_TEST(test_node_cache_round_trip_v6);
  CPPUNIT_TEST(test_node_cache_round_trip_v4);
  CPPUNIT_TEST(test_tracker_v6_value_is_18_bytes_network_order);
  CPPUNIT_TEST(test_tracker_v6_same_address_updates_port);
  CPPUNIT_TEST(test_values_mixed_v4_and_v6);
  CPPUNIT_TEST(test_values_truncated_or_odd_entries_are_skipped);
  CPPUNIT_TEST(test_transaction_key_v6);
  CPPUNIT_TEST(test_router_v6_token);
  CPPUNIT_TEST(test_router_v6_cache_keeps_families_apart);
  CPPUNIT_TEST(test_bucket_full_cache_holds_eight_nodes);
  CPPUNIT_TEST(test_message_reads_nodes_and_nodes6);
  CPPUNIT_TEST(test_announce_reaches_the_eight_closest);
  CPPUNIT_TEST(test_bootstrap_retries_within_seconds);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_node_compact_is_38_bytes();
  void test_node_compact_v4_is_still_26_bytes();
  void test_node_cache_round_trip_v6();
  void test_node_cache_round_trip_v4();
  void test_tracker_v6_value_is_18_bytes_network_order();
  void test_tracker_v6_same_address_updates_port();
  void test_values_mixed_v4_and_v6();
  void test_values_truncated_or_odd_entries_are_skipped();
  void test_transaction_key_v6();
  void test_router_v6_token();
  void test_router_v6_cache_keeps_families_apart();
  void test_bucket_full_cache_holds_eight_nodes();
  void test_message_reads_nodes_and_nodes6();
  void test_announce_reaches_the_eight_closest();
  void test_bootstrap_retries_within_seconds();
};
