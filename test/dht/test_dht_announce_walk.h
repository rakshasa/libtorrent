#include "test/helpers/test_main_thread.h"

class TestDhtAnnounceWalk : public TestFixtureWithMainNetTrackerThread {
  CPPUNIT_TEST_SUITE(TestDhtAnnounceWalk);

  CPPUNIT_TEST(test_empty_announce_reports_primary_failure);
  CPPUNIT_TEST(test_empty_announce_does_not_report_secondary_failure);
  CPPUNIT_TEST(test_announce_reaches_the_eight_closest);
  CPPUNIT_TEST(test_walk_sends_get_peers);
  CPPUNIT_TEST(test_announce_peer_goes_to_eight_closest_with_their_tokens);
  CPPUNIT_TEST(test_no_second_get_peers_round);
  CPPUNIT_TEST(test_walk_continues_after_quick_timeout);
  CPPUNIT_TEST(test_oversized_token_keeps_walking);
  CPPUNIT_TEST(test_incoming_ping_after_invalid_datagram);
  CPPUNIT_TEST(test_incoming_get_peers_without_nodes);
  CPPUNIT_TEST(test_incoming_malformed_query);
  CPPUNIT_TEST(test_malformed_response_reports_failed_search);
  CPPUNIT_TEST(test_malformed_response_does_not_fail_successful_search);
  CPPUNIT_TEST(test_udp_router_keyed_dispatch);
  CPPUNIT_TEST(test_udp_router_cleanup_on_exception);
  CPPUNIT_TEST(test_retry_and_reply);
  CPPUNIT_TEST(test_announce_retry_preserves_query);
  CPPUNIT_TEST(test_sent_transaction_timeout_marks_node_inactive);
  CPPUNIT_TEST(test_error_completes_transaction);
  CPPUNIT_TEST(test_timeout_bucket_pointer_stability);
  CPPUNIT_TEST(test_retry_deadline_order);
  CPPUNIT_TEST(test_tracker_retry_deadline_order);
  CPPUNIT_TEST(test_late_reply_after_cancel);
  CPPUNIT_TEST(test_late_reply_after_timeout);

  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() override;

  void test_empty_announce_reports_primary_failure();
  void test_empty_announce_does_not_report_secondary_failure();
  void test_announce_reaches_the_eight_closest();
  void test_walk_sends_get_peers();
  void test_announce_peer_goes_to_eight_closest_with_their_tokens();
  void test_no_second_get_peers_round();
  void test_walk_continues_after_quick_timeout();
  void test_oversized_token_keeps_walking();
  void test_incoming_ping_after_invalid_datagram();
  void test_incoming_get_peers_without_nodes();
  void test_incoming_malformed_query();
  void test_malformed_response_reports_failed_search();
  void test_malformed_response_does_not_fail_successful_search();
  void test_udp_router_keyed_dispatch();
  void test_udp_router_cleanup_on_exception();
  void test_retry_and_reply();
  void test_announce_retry_preserves_query();
  void test_sent_transaction_timeout_marks_node_inactive();
  void test_error_completes_transaction();
  void test_timeout_bucket_pointer_stability();
  void test_retry_deadline_order();
  void test_tracker_retry_deadline_order();
  void test_late_reply_after_cancel();
  void test_late_reply_after_timeout();
};
