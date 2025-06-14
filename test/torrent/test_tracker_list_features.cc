#include "config.h"

#include <functional>

#include "net/address_list.h"
#include "test/torrent/test_tracker_list.h"
#include "test/torrent/test_tracker_list_features.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestTrackerListFeatures);

namespace {
[[maybe_unused]] bool
verify_did_internal_error(std::function<void ()> func, bool should_throw) {
  bool did_throw = false;

  try {
    func();
  } catch (const torrent::internal_error&) {
    did_throw = true;
  }

  return should_throw == did_throw;
}
} // namespace

void
TestTrackerListFeatures::test_new_peers() {
  TRACKER_LIST_SETUP();
  TRACKER_INSERT(0, tracker_0_0);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);

  CPPUNIT_ASSERT(tracker_0_0.state().latest_new_peers() == 0);
  CPPUNIT_ASSERT(tracker_0_0.state().latest_sum_peers() == 0);

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success(10, 20));
  CPPUNIT_ASSERT(tracker_0_0.state().latest_new_peers() == 10);
  CPPUNIT_ASSERT(tracker_0_0.state().latest_sum_peers() == 20);

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_failure());
  CPPUNIT_ASSERT(tracker_0_0.state().latest_new_peers() == 10);
  CPPUNIT_ASSERT(tracker_0_0.state().latest_sum_peers() == 20);

  tracker_list.clear_stats();
  CPPUNIT_ASSERT(tracker_0_0.state().latest_new_peers() == 0);
  CPPUNIT_ASSERT(tracker_0_0.state().latest_sum_peers() == 0);
}

// test last_connect timer.


// test has_active, and then clean up TrackerManager.

void
TestTrackerListFeatures::test_has_active() {
  TRACKER_LIST_SETUP();

  TRACKER_INSERT(0, tracker_0_0);
  TRACKER_INSERT(0, tracker_0_1);
  TRACKER_INSERT(1, tracker_1_0);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  auto tracker_0_1_worker = TrackerTest::test_worker(tracker_0_1);
  auto tracker_1_0_worker = TrackerTest::test_worker(tracker_1_0);

  CPPUNIT_ASSERT(!tracker_list.has_active());
  CPPUNIT_ASSERT(!tracker_list.has_active_not_scrape());

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_list.has_active());
  CPPUNIT_ASSERT(tracker_list.has_active_not_scrape());
  tracker_0_0_worker->trigger_success();
  CPPUNIT_ASSERT(!tracker_list.has_active());
  CPPUNIT_ASSERT(!tracker_list.has_active_not_scrape());

  tracker_list.send_event(tracker_list.at(2), torrent::tracker::TrackerState::EVENT_NONE); CPPUNIT_ASSERT(tracker_list.has_active());
  tracker_1_0_worker->trigger_success(); CPPUNIT_ASSERT(!tracker_list.has_active());

  // Test multiple active trackers.
  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE); CPPUNIT_ASSERT(tracker_list.has_active());

  tracker_list.send_event(tracker_list.at(1), torrent::tracker::TrackerState::EVENT_NONE);
  tracker_0_0_worker->trigger_success(); CPPUNIT_ASSERT(tracker_list.has_active());
  tracker_0_1_worker->trigger_success(); CPPUNIT_ASSERT(!tracker_list.has_active());

  tracker_1_0_worker->set_scrapable();
  tracker_list.send_scrape(tracker_1_0);
  CPPUNIT_ASSERT(tracker_list.has_active());
  CPPUNIT_ASSERT(!tracker_list.has_active_not_scrape());
}

void
TestTrackerListFeatures::test_find_next_to_request() {
  TRACKER_LIST_SETUP();

  TRACKER_INSERT(0, tracker_0);
  TRACKER_INSERT(0, tracker_1);
  TRACKER_INSERT(0, tracker_2);
  TRACKER_INSERT(0, tracker_3);

  auto tracker_0_worker = TrackerTest::test_worker(tracker_0);
  auto tracker_1_worker = TrackerTest::test_worker(tracker_1);
  auto tracker_2_worker = TrackerTest::test_worker(tracker_2);
  auto tracker_3_worker = TrackerTest::test_worker(tracker_3);

  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin());
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin() + 1) == tracker_list.begin() + 1);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.end()) == tracker_list.end());

  tracker_0.disable();
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 1);

  tracker_0.enable();
  tracker_0_worker->set_failed(1, torrent::this_thread::cached_seconds().count() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 1);

  tracker_1_worker->set_failed(1, torrent::this_thread::cached_seconds().count() - 0);
  tracker_2_worker->set_failed(1, torrent::this_thread::cached_seconds().count() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 3);

  tracker_3_worker->set_failed(1, torrent::this_thread::cached_seconds().count() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 0);

  tracker_0_worker->set_failed(1, torrent::this_thread::cached_seconds().count() - 3);
  tracker_1_worker->set_failed(1, torrent::this_thread::cached_seconds().count() - 2);
  tracker_2_worker->set_failed(1, torrent::this_thread::cached_seconds().count() - 4);
  tracker_3_worker->set_failed(1, torrent::this_thread::cached_seconds().count() - 2);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 2);

  tracker_1_worker->set_failed(0, torrent::this_thread::cached_seconds().count() - 1);
  tracker_1_worker->set_success(1, torrent::this_thread::cached_seconds().count() - 1);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 0);
  tracker_1_worker->set_success(1, torrent::this_thread::cached_seconds().count() - (tracker_1.state().normal_interval() - 1));
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 1);
}

void
TestTrackerListFeatures::test_find_next_to_request_groups() {
  TRACKER_LIST_SETUP();

  TRACKER_INSERT(0, tracker_0);
  TRACKER_INSERT(0, tracker_1);
  TRACKER_INSERT(1, tracker_2);
  TRACKER_INSERT(1, tracker_3);

  auto tracker_0_worker = TrackerTest::test_worker(tracker_0);
  auto tracker_1_worker = TrackerTest::test_worker(tracker_1);
  auto tracker_2_worker = TrackerTest::test_worker(tracker_2);

  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin());

  tracker_0_worker->set_failed(1, torrent::this_thread::cached_seconds().count() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 1);

  tracker_1_worker->set_failed(1, torrent::this_thread::cached_seconds().count() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 2);

  tracker_2_worker->set_failed(1, torrent::this_thread::cached_seconds().count() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 3);

  tracker_1_worker->set_failed(0, torrent::this_thread::cached_seconds().count() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 1);
}

void
TestTrackerListFeatures::test_count_active() {
  TRACKER_LIST_SETUP();

  TRACKER_INSERT(0, tracker_0_0);
  TRACKER_INSERT(0, tracker_0_1);
  TRACKER_INSERT(1, tracker_1_0);
  TRACKER_INSERT(2, tracker_2_0);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  auto tracker_0_1_worker = TrackerTest::test_worker(tracker_0_1);
  auto tracker_1_0_worker = TrackerTest::test_worker(tracker_1_0);
  auto tracker_2_0_worker = TrackerTest::test_worker(tracker_2_0);

  CPPUNIT_ASSERT(TrackerTest::count_active(&tracker_list) == 0);

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(TrackerTest::count_active(&tracker_list) == 1);

  tracker_list.send_event(tracker_list.at(3), torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(TrackerTest::count_active(&tracker_list) == 2);

  tracker_list.send_event(tracker_list.at(1), torrent::tracker::TrackerState::EVENT_NONE);
  tracker_list.send_event(tracker_list.at(2), torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(TrackerTest::count_active(&tracker_list) == 4);

  tracker_0_0_worker->trigger_success();
  CPPUNIT_ASSERT(TrackerTest::count_active(&tracker_list) == 3);

  tracker_0_1_worker->trigger_success();
  tracker_2_0_worker->trigger_success();
  CPPUNIT_ASSERT(TrackerTest::count_active(&tracker_list) == 1);

  tracker_1_0_worker->trigger_success();
  CPPUNIT_ASSERT(TrackerTest::count_active(&tracker_list) == 0);
}

// Add separate functions for sending state to multiple trackers...


void
TestTrackerListFeatures::test_request_safeguard() {
  // TODO: Reimplement tracker hammering properly.

  // TRACKER_LIST_SETUP();
  // TRACKER_INSERT(0, tracker_1);
  // TRACKER_INSERT(0, tracker_2);
  // TRACKER_INSERT(0, tracker_3);
  // TRACKER_INSERT(0, tracker_foo);

  // auto tracker_1_worker = TrackerTest::test_worker(tracker_1);
  // auto tracker_2_worker = TrackerTest::test_worker(tracker_2);
  // auto tracker_3_worker = TrackerTest::test_worker(tracker_3);
  // auto tracker_foo_worker = TrackerTest::test_worker(tracker_foo);

  // for (unsigned int i = 0; i < 9; i++) {
  //   CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_event, &tracker_list, tracker_1, torrent::tracker::TrackerState::EVENT_NONE), false));
  //   CPPUNIT_ASSERT(tracker_1_worker->trigger_success());
  //   CPPUNIT_ASSERT(tracker_1.state().success_counter() == (i + 1));
  // }

  // CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_event, &tracker_list, tracker_1, torrent::tracker::TrackerState::EVENT_NONE), true));
  // CPPUNIT_ASSERT(tracker_1_worker->trigger_success());

  // cached_time += rak::timer::from_seconds(1000);

  // for (unsigned int i = 0; i < 9; i++) {
  //   CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_event, &tracker_list, tracker_foo, torrent::tracker::TrackerState::EVENT_NONE), false));
  //   CPPUNIT_ASSERT(tracker_foo_worker->trigger_success());
  //   CPPUNIT_ASSERT(tracker_foo.state().success_counter() == (i + 1));
  //   CPPUNIT_ASSERT(tracker_foo->is_usable());
  // }

  // CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_event, &tracker_list, tracker_foo, torrent::tracker::TrackerState::EVENT_NONE), true));
  // CPPUNIT_ASSERT(tracker_foo_worker->trigger_success());

  // for (unsigned int i = 0; i < 40; i++) {
  //   CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_event, &tracker_list, tracker_2, torrent::tracker::TrackerState::EVENT_NONE), false));
  //   CPPUNIT_ASSERT(tracker_2_worker->trigger_success());
  //   CPPUNIT_ASSERT(tracker_2.state().success_counter() == (i + 1));

  //   cached_time += rak::timer::from_seconds(1);
  // }

  // for (unsigned int i = 0; i < 17; i++) {
  //   CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_event, &tracker_list, tracker_3, torrent::tracker::TrackerState::EVENT_NONE), false));
  //   CPPUNIT_ASSERT(tracker_3_worker->trigger_success());
  //   CPPUNIT_ASSERT(tracker_3.state().success_counter() == (i + 1));

  //   if (i % 2)
  //     cached_time += rak::timer::from_seconds(1);
  // }

  // CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_event, &tracker_list, tracker_3, torrent::tracker::TrackerState::EVENT_NONE), true));
  // CPPUNIT_ASSERT(tracker_3_worker->trigger_success());

  // TRACKER_LIST_CLEANUP();
}
