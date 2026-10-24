#ifndef LIBTORRENT_TEST_NET_TEST_LISTEN_H
#define LIBTORRENT_TEST_NET_TEST_LISTEN_H

#include "helpers/test_main_thread.h"

class test_listen : public TestFixtureWithMockAndMainThread {
  CPPUNIT_TEST_SUITE(test_listen);
  CPPUNIT_TEST(test_second_stream_failure);
  CPPUNIT_TEST(test_second_datagram_failure);
  CPPUNIT_TEST(test_first_stream_failure);
  CPPUNIT_TEST(test_listen_failure);
  CPPUNIT_TEST(test_retry_failure);
  CPPUNIT_TEST(test_retry_close_failure);
  CPPUNIT_TEST(test_cleanup_close_failure);
  CPPUNIT_TEST(test_range_exhausted);
  CPPUNIT_TEST(test_invalid_address);
  CPPUNIT_TEST(test_open_success);
  CPPUNIT_TEST_SUITE_END();

public:
  void test_second_stream_failure();
  void test_second_datagram_failure();
  void test_first_stream_failure();
  void test_listen_failure();
  void test_retry_failure();
  void test_retry_close_failure();
  void test_cleanup_close_failure();
  void test_range_exhausted();
  void test_invalid_address();
  void test_open_success();
};

#endif
