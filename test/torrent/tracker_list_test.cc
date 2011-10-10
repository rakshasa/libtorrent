#include "config.h"

#include "tracker_list_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(tracker_list_test);

void
tracker_list_test::test_basic() {
  torrent::TrackerList tracker_list;

  tracker_list.insert(0, new TrackerTest(&tracker_list, ""));

  CPPUNIT_ASSERT(tracker_list[0]->parent() == &tracker_list);
  CPPUNIT_ASSERT(std::distance(tracker_list.begin_group(0), tracker_list.end_group(0)) == 1);
  CPPUNIT_ASSERT(tracker_list.find_usable(tracker_list.begin()) != tracker_list.end());
}

void
tracker_list_test::test_single_tracker() {
  torrent::TrackerList tracker_list;
  TrackerTest* tracker_0 = new TrackerTest(&tracker_list, "");

  tracker_list.insert(0, tracker_0);
  tracker_list.send_state(1);

  CPPUNIT_ASSERT(tracker_0->requesting_state() == 1);
}
