#ifndef LIBTORRENT_TEST_NET_TEST_CURL_GET_H
#define LIBTORRENT_TEST_NET_TEST_CURL_GET_H

#include "helpers/test_main_thread.h"

class test_curl_get : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_curl_get);

  CPPUNIT_TEST(test_start_get_after_close);
  CPPUNIT_TEST(test_slots_do_not_retain_stream);
  CPPUNIT_TEST(test_max_file_size_has_default);
  CPPUNIT_TEST(test_write_aborts_past_max_file_size);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_start_get_after_close();
  void test_slots_do_not_retain_stream();
  void test_max_file_size_has_default();
  void test_write_aborts_past_max_file_size();
};

#endif
