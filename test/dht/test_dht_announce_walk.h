#include "test/helpers/test_main_thread.h"

class TestDhtAnnounceWalk : public TestFixtureWithMainNetTrackerThread {
  CPPUNIT_TEST_SUITE(TestDhtAnnounceWalk);

  CPPUNIT_TEST(test_announce_reaches_the_eight_closest);
  CPPUNIT_TEST(test_walk_sends_get_peers);
  CPPUNIT_TEST(test_announce_peer_goes_to_eight_closest_with_their_tokens);
  CPPUNIT_TEST(test_no_second_get_peers_round);
  CPPUNIT_TEST(test_walk_continues_after_quick_timeout);
  CPPUNIT_TEST(test_oversized_token_keeps_walking);

  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() override;

  void test_announce_reaches_the_eight_closest();
  void test_walk_sends_get_peers();
  void test_announce_peer_goes_to_eight_closest_with_their_tokens();
  void test_no_second_get_peers_round();
  void test_walk_continues_after_quick_timeout();
  void test_oversized_token_keeps_walking();
};
