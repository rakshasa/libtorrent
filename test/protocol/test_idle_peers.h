#include "test/helpers/test_main_thread.h"

class test_idle_peers : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_idle_peers);

  CPPUNIT_TEST(test_clock_counts_while_choked);
  CPPUNIT_TEST(test_clock_counts_unchoked_silent);
  CPPUNIT_TEST(test_clock_reset_by_piece_data);
  CPPUNIT_TEST(test_clock_reset_by_upload);
  CPPUNIT_TEST(test_clock_reset_by_unchoke);
  CPPUNIT_TEST(test_clock_reset_when_not_interested);
  CPPUNIT_TEST(test_clock_paused_while_our_slots_are_full);
  CPPUNIT_TEST(test_clock_paused_while_it_wants_our_pieces);
  CPPUNIT_TEST(test_idle_drop_refused_within_window);
  CPPUNIT_TEST(test_plain_disconnect_not_refused);
  CPPUNIT_TEST(test_stall_steady_rate);
  CPPUNIT_TEST(test_stall_nothing_received);
  CPPUNIT_TEST(test_stall_far_below_best);
  CPPUNIT_TEST(test_stall_one_bad_tick);
  CPPUNIT_TEST(test_stall_slow_but_steady);
  CPPUNIT_TEST(test_stall_forgets_old_best);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_clock_counts_while_choked();
  void test_clock_counts_unchoked_silent();
  void test_clock_reset_by_piece_data();
  void test_clock_reset_by_upload();
  void test_clock_reset_by_unchoke();
  void test_clock_reset_when_not_interested();
  void test_clock_paused_while_our_slots_are_full();
  void test_clock_paused_while_it_wants_our_pieces();
  void test_idle_drop_refused_within_window();
  void test_plain_disconnect_not_refused();
  void test_stall_steady_rate();
  void test_stall_nothing_received();
  void test_stall_far_below_best();
  void test_stall_one_bad_tick();
  void test_stall_slow_but_steady();
  void test_stall_forgets_old_best();
};
