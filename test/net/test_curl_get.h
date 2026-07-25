#ifndef LIBTORRENT_TEST_NET_TEST_CURL_GET_H
#define LIBTORRENT_TEST_NET_TEST_CURL_GET_H

#include "helpers/test_main_thread.h"

class test_curl_get : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_curl_get);

  CPPUNIT_TEST(test_start_get_after_close);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_start_get_after_close();
};

#endif
