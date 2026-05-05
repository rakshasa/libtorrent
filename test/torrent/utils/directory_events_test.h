#ifndef LIBTORRENT_TEST_DIRECTORY_EVENTS_TEST_H
#define LIBTORRENT_TEST_DIRECTORY_EVENTS_TEST_H

#include <string>

#include "test/helpers/test_main_thread.h"
#include "torrent/utils/directory_events.h"

class utils_directory_events_test : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(utils_directory_events_test);
  CPPUNIT_TEST(test_ready_conflicts_with_added_same_directory);
  CPPUNIT_TEST(test_added_conflicts_with_ready_same_directory_normalized);
  CPPUNIT_TEST(test_ready_allows_added_child);
  CPPUNIT_TEST(test_added_allows_ready_parent);
  CPPUNIT_TEST(test_ready_allows_added_sibling);
  CPPUNIT_TEST(test_symlink_watch_uses_configured_callback_path);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_ready_conflicts_with_added_same_directory();
  void test_added_conflicts_with_ready_same_directory_normalized();
  void test_ready_allows_added_child();
  void test_added_allows_ready_parent();
  void test_ready_allows_added_sibling();
  void test_symlink_watch_uses_configured_callback_path();

private:
  std::string m_link;
  std::string m_root;
};

#endif
