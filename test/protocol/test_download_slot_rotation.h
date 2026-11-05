#include "test/helpers/test_main_thread.h"

class test_download_slot_rotation : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_download_slot_rotation);

  CPPUNIT_TEST(test_count_follows_max_alternate);
  CPPUNIT_TEST(test_cycle_rotates_full_torrent);
  CPPUNIT_TEST(test_cycle_limited_by_the_queue);
  CPPUNIT_TEST(test_cycle_spares_the_newcomer);
  CPPUNIT_TEST(test_free_slot_is_left_to_the_cycle);
  CPPUNIT_TEST(test_unlimited_is_left_alone);
  CPPUNIT_TEST(test_nothing_waiting);
  CPPUNIT_TEST(test_balance_does_not_rotate);
  CPPUNIT_TEST(test_interest_restored_after_remote_choke);
  CPPUNIT_TEST(test_interest_not_restored_when_done);
  CPPUNIT_TEST(test_interest_kept_after_rotation);
  CPPUNIT_TEST(test_interest_untouched_when_not_withheld);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_count_follows_max_alternate();
  void test_cycle_rotates_full_torrent();
  void test_cycle_limited_by_the_queue();
  void test_cycle_spares_the_newcomer();
  void test_free_slot_is_left_to_the_cycle();
  void test_unlimited_is_left_alone();
  void test_nothing_waiting();
  void test_balance_does_not_rotate();
  void test_interest_restored_after_remote_choke();
  void test_interest_not_restored_when_done();
  void test_interest_kept_after_rotation();
  void test_interest_untouched_when_not_withheld();
};
