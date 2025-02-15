#include "config.h"

#include <iostream>
#include <torrent/tracker_controller.h>

#include "rak/priority_queue_default.h"

#include "globals.h"
#include "test/torrent/test_tracker_list.h"
#include "test/torrent/test_tracker_timeout.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestTrackerTimeout);

void
TestTrackerTimeout::setUp() {
  torrent::cachedTime = rak::timer::current();
  //  torrent::cachedTime = rak::timer::current().round_seconds();

  test_fixture::setUp();
}

void
TestTrackerTimeout::test_set_timeout() {
  TrackerTest tracker(NULL, "");

  CPPUNIT_ASSERT(tracker.state().normal_interval() == 0);

  tracker.set_new_normal_interval(100);
  CPPUNIT_ASSERT(tracker.state().normal_interval() == 600);
  tracker.set_new_normal_interval(8 * 4000);
  CPPUNIT_ASSERT(tracker.state().normal_interval() == 8 * 3600);

  tracker.set_new_min_interval(100);
  CPPUNIT_ASSERT_EQUAL((uint32_t)300, tracker.state().min_interval());
  tracker.set_new_min_interval(4 * 4000);
  CPPUNIT_ASSERT(tracker.state().min_interval() == 4 * 3600);
}

void
TestTrackerTimeout::test_timeout_tracker() {
  auto tracker = std::unique_ptr<torrent::Tracker>(TestTrackerList::new_tracker(NULL, "http://tracker.com/announce"));
  auto tracker_worker = dynamic_cast<TrackerTest*>(tracker->m_worker.get());

  int flags = 0;

  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 0);
  torrent::cachedTime += rak::timer::from_seconds(3);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 0);

  flags = torrent::TrackerController::flag_active;

  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 0);
  tracker_worker->send_event(torrent::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == ~uint32_t());
  tracker_worker->send_event(torrent::TrackerState::EVENT_SCRAPE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 0);

  tracker_worker->close();
  tracker_worker->set_success(1, torrent::cachedTime.seconds());

  // Check also failed...

  CPPUNIT_ASSERT_EQUAL((uint32_t)1800, torrent::tracker_next_timeout(tracker.get(), flags));
  tracker_worker->send_event(torrent::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == ~uint32_t());
  tracker_worker->send_event(torrent::TrackerState::EVENT_SCRAPE);
  CPPUNIT_ASSERT_EQUAL((uint32_t)1800, torrent::tracker_next_timeout(tracker.get(), flags));

  tracker_worker->close();

  tracker_worker->set_success(1, torrent::cachedTime.seconds() - 3);
  CPPUNIT_ASSERT_EQUAL((uint32_t)(1800 - 3), torrent::tracker_next_timeout(tracker.get(), flags));
  tracker_worker->set_success(1, torrent::cachedTime.seconds() + 3);
  CPPUNIT_ASSERT_EQUAL((uint32_t)(1800 + 3), torrent::tracker_next_timeout(tracker.get(), flags));

  tracker_worker->close();
  flags = torrent::TrackerController::flag_active | torrent::TrackerController::flag_promiscuous_mode;

  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 0);
  tracker_worker->send_event(torrent::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == ~uint32_t());
  tracker_worker->send_event(torrent::TrackerState::EVENT_SCRAPE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 0);
}

void
TestTrackerTimeout::test_timeout_update() {
  auto tracker = std::unique_ptr<torrent::Tracker>(TestTrackerList::new_tracker(NULL, "http://tracker.com/announce"));
  auto tracker_worker = dynamic_cast<TrackerTest*>(tracker->m_worker.get());

  int flags = 0;

  flags = torrent::TrackerController::flag_active | torrent::TrackerController::flag_send_update;

  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 0);
  tracker_worker->send_event(torrent::TrackerState::EVENT_SCRAPE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 0);
  tracker_worker->send_event(torrent::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == ~uint32_t());

  tracker_worker->close();
  tracker_worker->set_failed(1, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 0);

  tracker_worker->set_failed(0, torrent::cachedTime.seconds());
  tracker_worker->set_success(0, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 0);
}

void
TestTrackerTimeout::test_timeout_requesting() {
  auto tracker = std::unique_ptr<torrent::Tracker>(TestTrackerList::new_tracker(NULL, "http://tracker.com/announce"));
  auto tracker_worker = dynamic_cast<TrackerTest*>(tracker->m_worker.get());

  int flags = 0;

  flags = torrent::TrackerController::flag_active | torrent::TrackerController::flag_requesting;

  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 0);
  tracker_worker->send_event(torrent::TrackerState::EVENT_SCRAPE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 0);
  tracker_worker->send_event(torrent::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == ~uint32_t());

  // tracker_worker->set_latest_new_peers(10 - 1);

  tracker_worker->close();
  tracker_worker->set_failed(1, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 5);
  tracker_worker->set_failed(2, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 10);
  tracker_worker->set_failed(6 + 1, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT_EQUAL((uint32_t)299, torrent::tracker_next_timeout(tracker.get(), flags));
  tracker_worker->set_failed(7 + 1, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT_EQUAL((uint32_t)299, torrent::tracker_next_timeout(tracker.get(), flags));

  //std::cout << "timeout:" << torrent::tracker_next_timeout(tracker.get(), flags) << std::endl;

  tracker_worker->set_failed(0, torrent::cachedTime.seconds());
  tracker_worker->set_success(0, torrent::cachedTime.seconds());
  // CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 10);
  // tracker_worker->set_success(1, torrent::cachedTime.seconds());
  // CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 20);
  // tracker_worker->set_success(2, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 600);
  tracker_worker->set_success(6, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 600);

  tracker_worker->set_success(1, torrent::cachedTime.seconds());
  // tracker_worker->set_latest_sum_peers(9);
  // CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 20);
  tracker_worker->set_latest_sum_peers(10);
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 600);

  tracker_worker->set_latest_sum_peers(10);
  tracker_worker->set_latest_new_peers(10);
  tracker_worker->set_success(1, torrent::cachedTime.seconds());
  CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker.get(), flags) == 600);
}
