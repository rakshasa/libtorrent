#include "test/helpers/test_main_thread.h"

class test_peer_redial : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_peer_redial);

  CPPUNIT_TEST(test_wanted_only_for_closes_we_did_not_choose);
  CPPUNIT_TEST(test_wanted_only_while_downloading);
  CPPUNIT_TEST(test_wanted_for_dialled_or_sending_peers);
  CPPUNIT_TEST(test_queued_on_listen_port);
  CPPUNIT_TEST(test_incoming_queued_on_listen_port);
  CPPUNIT_TEST(test_incoming_without_listen_port_not_queued);
  CPPUNIT_TEST(test_inet6_queued_on_listen_port);
  CPPUNIT_TEST(test_queued_peer_passes_recent_filter);
  CPPUNIT_TEST(test_already_listed_peer_passes_recent_filter);
  CPPUNIT_TEST(test_once_per_interval);
  CPPUNIT_TEST(test_not_for_bad_or_banned_peers);
  CPPUNIT_TEST(test_not_past_list_limit);
  CPPUNIT_TEST(test_plain_disconnect_not_queued);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_wanted_only_for_closes_we_did_not_choose();
  void test_wanted_only_while_downloading();
  void test_wanted_for_dialled_or_sending_peers();
  void test_queued_on_listen_port();
  void test_incoming_queued_on_listen_port();
  void test_incoming_without_listen_port_not_queued();
  void test_inet6_queued_on_listen_port();
  void test_queued_peer_passes_recent_filter();
  void test_already_listed_peer_passes_recent_filter();
  void test_once_per_interval();
  void test_not_for_bad_or_banned_peers();
  void test_not_past_list_limit();
  void test_plain_disconnect_not_queued();
};
