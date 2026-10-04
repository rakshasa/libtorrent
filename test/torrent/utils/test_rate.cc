#include "config.h"

#include "test_rate.h"

#include "torrent/rate.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_rate, "torrent/utils");

void
test_rate::test_startup_and_filling_window() {
  m_main_thread->test_set_cached_time(0s);
  torrent::Rate rate(4);

  rate.insert(100);
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::rate_type{100}, rate.rate());

  m_main_thread->test_add_cached_time(1s);
  rate.insert(100);
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::rate_type{100}, rate.rate());

  m_main_thread->test_add_cached_time(1s);
  rate.insert(100);
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::rate_type{100}, rate.rate());

  m_main_thread->test_add_cached_time(1s);
  rate.insert(100);
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::rate_type{100}, rate.rate());

  m_main_thread->test_add_cached_time(1s);
  rate.insert(100);
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::rate_type{100}, rate.rate());
}

void
test_rate::test_rate_decays_after_span() {
  m_main_thread->test_set_cached_time(0s);
  torrent::Rate rate(5);

  rate.insert(500);
  m_main_thread->test_add_cached_time(5s);

  CPPUNIT_ASSERT_EQUAL(torrent::Rate::rate_type{0}, rate.rate());
}

void
test_rate::test_restart_after_idle() {
  m_main_thread->test_set_cached_time(0s);
  torrent::Rate rate(30);
  rate.set_startup_span(3);

  rate.insert(1200);
  m_main_thread->test_add_cached_time(1s);
  rate.insert(1200);

  m_main_thread->test_add_cached_time(15s);
  CPPUNIT_ASSERT(rate.rate() < 1200);

  rate.insert(1200);
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::rate_type{1200}, rate.rate());
}

void
test_rate::test_set_span_and_reset() {
  m_main_thread->test_set_cached_time(0s);
  torrent::Rate rate(5);

  rate.insert(100);
  rate.set_span(2);

  CPPUNIT_ASSERT_EQUAL(torrent::Rate::timer_type{2}, rate.span());
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::rate_type{0}, rate.rate());
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::total_type{100}, rate.total());

  rate.insert(200);
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::rate_type{200}, rate.rate());

  rate.reset_rate();
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::rate_type{0}, rate.rate());
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::total_type{300}, rate.total());

  rate.insert(300);
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::rate_type{300}, rate.rate());
}

void
test_rate::test_startup_span() {
  torrent::Rate rate(30);

  CPPUNIT_ASSERT_EQUAL(torrent::Rate::timer_type{3}, rate.startup_span());

  rate.set_startup_span(5);
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::timer_type{5}, rate.startup_span());

  rate.set_startup_span(60);
  CPPUNIT_ASSERT_EQUAL(torrent::Rate::timer_type{30}, rate.startup_span());
}
