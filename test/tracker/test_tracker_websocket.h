#ifndef LIBTORRENT_TEST_TRACKER_WEBSOCKET_H
#define LIBTORRENT_TEST_TRACKER_WEBSOCKET_H

#include <cppunit/extensions/HelperMacros.h>

#include "test/helpers/test_main_thread.h"

class test_tracker_websocket : public TestFixtureWithMainNetTrackerThread {
  CPPUNIT_TEST_SUITE(test_tracker_websocket);

#ifdef USE_WEBTORRENT
  CPPUNIT_TEST(test_insert_url);
  CPPUNIT_TEST(test_parse_announce);
  CPPUNIT_TEST(test_parse_offer);
  CPPUNIT_TEST(test_parse_answer);
  CPPUNIT_TEST(test_parse_invalid);
  CPPUNIT_TEST(test_parse_binary_hash);
  CPPUNIT_TEST(test_build_announce);
  CPPUNIT_TEST(test_build_answer);
  CPPUNIT_TEST(test_rtc_signaling);
#endif

  CPPUNIT_TEST_SUITE_END();

public:
  void test_insert_url();
  void test_parse_announce();
  void test_parse_offer();
  void test_parse_answer();
  void test_parse_invalid();
  void test_parse_binary_hash();
  void test_build_announce();
  void test_build_answer();
  void test_rtc_signaling();
};

#endif // LIBTORRENT_TEST_TRACKER_WEBSOCKET_H
