#ifndef LIBTORRENT_TEST_NET_TEST_EVENT_FD_H
#define LIBTORRENT_TEST_NET_TEST_EVENT_FD_H

#include "helpers/test_main_thread.h"

class test_event_fd : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_event_fd);

  CPPUNIT_TEST(test_destroy_closes_descriptor);
  CPPUNIT_TEST(test_destroy_closes_standard_descriptor);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_destroy_closes_descriptor();
  void test_destroy_closes_standard_descriptor();
};

#endif
