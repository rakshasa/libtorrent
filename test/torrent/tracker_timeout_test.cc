#include "config.h"

#include <iostream>
#include <torrent/tracker_controller.h>

#include "rak/priority_queue_default.h"

#include "globals.h"
#include "tracker_list_test.h"
#include "tracker_timeout_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(tracker_timeout_test);

void
tracker_timeout_test::setUp() {
  torrent::cachedTime = rak::timer::current();
  //  torrent::cachedTime = rak::timer::current().round_seconds();
}

void
tracker_timeout_test::tearDown() {
}

void
tracker_timeout_test::test_set_timeout() {
  TrackerTest tracker(NULL, "");

  CPPUNIT_ASSERT(tracker.normal_interval() == 1800);

  tracker.set_new_normal_interval(100);
  CPPUNIT_ASSERT(tracker.normal_interval() == 600);
  tracker.set_new_normal_interval(8 * 4000);
  CPPUNIT_ASSERT(tracker.normal_interval() == 8 * 3600);

  tracker.set_new_min_interval(100);
  CPPUNIT_ASSERT(tracker.min_interval() == 300);
  tracker.set_new_min_interval(4 * 4000);
  CPPUNIT_ASSERT(tracker.min_interval() == 4 * 3600);
}

void
tracker_timeout_test::test_timeout_tracker() {
  TrackerTest tracker(NULL, "");
  int flags = 0;

  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 0);
  torrent::cachedTime += rak::timer::from_seconds(3);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 0);

  flags = torrent::TrackerController::flag_active;

  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 0);
  tracker.send_state(torrent::Tracker::EVENT_NONE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == ~uint32_t());
  tracker.send_state(torrent::Tracker::EVENT_SCRAPE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 0);

  tracker.close();
  tracker.set_success(1, torrent::cachedTime.seconds());

  // Check also failed...

  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 1800);
  tracker.send_state(torrent::Tracker::EVENT_NONE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == ~uint32_t());
  tracker.send_state(torrent::Tracker::EVENT_SCRAPE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 1800);

  tracker.close();

  tracker.set_success(1, torrent::cachedTime.seconds() - 3);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 1800 - 3);
  tracker.set_success(1, torrent::cachedTime.seconds() + 3);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 1800 + 3);

  tracker.close();
  flags = torrent::TrackerController::flag_active | torrent::TrackerController::flag_promiscuous_mode;

  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 0);
  tracker.send_state(torrent::Tracker::EVENT_NONE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == ~uint32_t());
  tracker.send_state(torrent::Tracker::EVENT_SCRAPE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 0);
}

void
tracker_timeout_test::test_timeout_update() {
  TrackerTest tracker(NULL, "");
  int flags = 0;

  flags = torrent::TrackerController::flag_active | torrent::TrackerController::flag_send_update;

  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 0);
  tracker.send_state(torrent::Tracker::EVENT_SCRAPE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 0);
  tracker.send_state(torrent::Tracker::EVENT_NONE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == ~uint32_t());

  tracker.close();
  tracker.set_failed(1, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 0);

  tracker.set_failed(0, torrent::cachedTime.seconds());
  tracker.set_success(0, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 0);
}

void
tracker_timeout_test::test_timeout_requesting() {
  TrackerTest tracker(NULL, "");
  int flags = 0;

  flags = torrent::TrackerController::flag_active | torrent::TrackerController::flag_requesting;

  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 0);
  tracker.send_state(torrent::Tracker::EVENT_SCRAPE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 0);
  tracker.send_state(torrent::Tracker::EVENT_NONE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == ~uint32_t());

  // tracker.set_latest_new_peers(10 - 1);

  tracker.close();
  tracker.set_failed(1, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 5);
  tracker.set_failed(2, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 10);
  tracker.set_failed(6 + 1, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 320);
  tracker.set_failed(7 + 1, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 320);
  
  //std::cout << "timeout:" << torrent::tracker_next_timeout(&tracker, flags) << std::endl;

  tracker.set_failed(0, torrent::cachedTime.seconds());
  tracker.set_success(0, torrent::cachedTime.seconds());
  // CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 10);
  // tracker.set_success(1, torrent::cachedTime.seconds());
  // CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 20);
  // tracker.set_success(2, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 600);
  tracker.set_success(6, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 600);
  
  tracker.set_success(1, torrent::cachedTime.seconds());
  // tracker.set_latest_sum_peers(9);
  // CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 20);
  tracker.set_latest_sum_peers(10);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 600);

  tracker.set_latest_sum_peers(10);
  tracker.set_latest_new_peers(10);
  tracker.set_success(1, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(&tracker, flags) == 600);
}
