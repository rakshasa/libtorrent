#include "helpers/test_main_thread.h"

class test_rate : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_rate);

  CPPUNIT_TEST(test_startup_and_filling_window);
  CPPUNIT_TEST(test_rate_decays_after_span);
  CPPUNIT_TEST(test_restart_after_idle);
  CPPUNIT_TEST(test_zero_bytes_do_not_prevent_idle_restart);
  CPPUNIT_TEST(test_zero_bytes_do_not_discard_samples);
  CPPUNIT_TEST(test_clock_rollback_resets_rate);
  CPPUNIT_TEST(test_startup_span);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_startup_and_filling_window();
  void test_rate_decays_after_span();
  void test_restart_after_idle();
  void test_zero_bytes_do_not_prevent_idle_restart();
  void test_zero_bytes_do_not_discard_samples();
  void test_clock_rollback_resets_rate();
  void test_startup_span();
};
