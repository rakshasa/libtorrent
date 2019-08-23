#include "config.h"

#include <functional>

#include "torrent/http.h"
#include "net/address_list.h"

#include "globals.h"
#include "tracker_list_test.h"
#include "tracker_list_features_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(tracker_list_features_test);

void
tracker_list_features_test::setUp() {
  CPPUNIT_ASSERT(torrent::taskScheduler.empty());

  torrent::cachedTime = rak::timer::current();
}

void
tracker_list_features_test::tearDown() {
}

void
tracker_list_features_test::test_new_peers() {
  TRACKER_SETUP();
  TRACKER_INSERT(0, tracker_0);
  
  CPPUNIT_ASSERT(tracker_0->latest_new_peers() == 0);
  CPPUNIT_ASSERT(tracker_0->latest_sum_peers() == 0);

  tracker_list.send_state_idx(0, torrent::Tracker::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_0->trigger_success(10, 20));
  CPPUNIT_ASSERT(tracker_0->latest_new_peers() == 10);
  CPPUNIT_ASSERT(tracker_0->latest_sum_peers() == 20);

  tracker_list.send_state_idx(0, torrent::Tracker::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_0->trigger_failure());
  CPPUNIT_ASSERT(tracker_0->latest_new_peers() == 10);
  CPPUNIT_ASSERT(tracker_0->latest_sum_peers() == 20);

  tracker_list.clear_stats();
  CPPUNIT_ASSERT(tracker_0->latest_new_peers() == 0);
  CPPUNIT_ASSERT(tracker_0->latest_sum_peers() == 0);
}

// test last_connect timer.


// test has_active, and then clean up TrackerManager.

void
tracker_list_features_test::test_has_active() {
  TRACKER_SETUP();
  TRACKER_INSERT(0, tracker_0_0);
  TRACKER_INSERT(0, tracker_0_1);
  TRACKER_INSERT(1, tracker_1_0);

  CPPUNIT_ASSERT(!tracker_list.has_active());
  CPPUNIT_ASSERT(!tracker_list.has_active_not_scrape());

  tracker_list.send_state_idx(0, 1);
  CPPUNIT_ASSERT(tracker_list.has_active());
  CPPUNIT_ASSERT(tracker_list.has_active_not_scrape());
  tracker_0_0->trigger_success();
  CPPUNIT_ASSERT(!tracker_list.has_active());
  CPPUNIT_ASSERT(!tracker_list.has_active_not_scrape());

  tracker_list.send_state_idx(2, 1); CPPUNIT_ASSERT(tracker_list.has_active());
  tracker_1_0->trigger_success(); CPPUNIT_ASSERT(!tracker_list.has_active());

  // Test multiple active trackers.
  tracker_list.send_state_idx(0, 1); CPPUNIT_ASSERT(tracker_list.has_active());

  tracker_list.send_state_idx(1, 1);
  tracker_0_0->trigger_success(); CPPUNIT_ASSERT(tracker_list.has_active());
  tracker_0_1->trigger_success(); CPPUNIT_ASSERT(!tracker_list.has_active());

  tracker_1_0->set_can_scrape();
  tracker_list.send_scrape(tracker_1_0);
  CPPUNIT_ASSERT(tracker_list.has_active());
  CPPUNIT_ASSERT(!tracker_list.has_active_not_scrape());
}

void
tracker_list_features_test::test_find_next_to_request() {
  TRACKER_SETUP();
  TRACKER_INSERT(0, tracker_0);
  TRACKER_INSERT(0, tracker_1);
  TRACKER_INSERT(0, tracker_2);
  TRACKER_INSERT(0, tracker_3);

  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin());
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin() + 1) == tracker_list.begin() + 1);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.end()) == tracker_list.end());

  tracker_0->disable();
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 1);

  tracker_0->enable();
  tracker_0->set_failed(1, torrent::cachedTime.seconds() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 1);
  
  tracker_1->set_failed(1, torrent::cachedTime.seconds() - 0);
  tracker_2->set_failed(1, torrent::cachedTime.seconds() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 3);

  tracker_3->set_failed(1, torrent::cachedTime.seconds() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 0);

  tracker_0->set_failed(1, torrent::cachedTime.seconds() - 3);
  tracker_1->set_failed(1, torrent::cachedTime.seconds() - 2);
  tracker_2->set_failed(1, torrent::cachedTime.seconds() - 4);
  tracker_3->set_failed(1, torrent::cachedTime.seconds() - 2);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 2);

  tracker_1->set_failed(0, torrent::cachedTime.seconds() - 1);
  tracker_1->set_success(1, torrent::cachedTime.seconds() - 1);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 0);
  tracker_1->set_success(1, torrent::cachedTime.seconds() - (tracker_1->normal_interval() - 1));
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 1);
}

void
tracker_list_features_test::test_find_next_to_request_groups() {
  TRACKER_SETUP();
  TRACKER_INSERT(0, tracker_0);
  TRACKER_INSERT(0, tracker_1);
  TRACKER_INSERT(1, tracker_2);
  TRACKER_INSERT(1, tracker_3);

  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin());

  tracker_0->set_failed(1, torrent::cachedTime.seconds() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 1);

  tracker_1->set_failed(1, torrent::cachedTime.seconds() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 2);

  tracker_2->set_failed(1, torrent::cachedTime.seconds() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 3);

  tracker_1->set_failed(0, torrent::cachedTime.seconds() - 0);
  CPPUNIT_ASSERT(tracker_list.find_next_to_request(tracker_list.begin()) == tracker_list.begin() + 1);
}

void
tracker_list_features_test::test_count_active() {
  TRACKER_SETUP();
  TRACKER_INSERT(0, tracker_0_0);
  TRACKER_INSERT(0, tracker_0_1);
  TRACKER_INSERT(1, tracker_1_0);
  TRACKER_INSERT(2, tracker_2_0);

  CPPUNIT_ASSERT(tracker_list.count_active() == 0);

  tracker_list.send_state_idx(0, 1);
  CPPUNIT_ASSERT(tracker_list.count_active() == 1);

  tracker_list.send_state_idx(3, 1);
  CPPUNIT_ASSERT(tracker_list.count_active() == 2);

  tracker_list.send_state_idx(1, 1);
  tracker_list.send_state_idx(2, 1);
  CPPUNIT_ASSERT(tracker_list.count_active() == 4);

  tracker_0_0->trigger_success();
  CPPUNIT_ASSERT(tracker_list.count_active() == 3);
  
  tracker_0_1->trigger_success();
  tracker_2_0->trigger_success();
  CPPUNIT_ASSERT(tracker_list.count_active() == 1);
  
  tracker_1_0->trigger_success();
  CPPUNIT_ASSERT(tracker_list.count_active() == 0);
}

// Add separate functions for sending state to multiple trackers...


bool
verify_did_internal_error(std::function<void ()> func, bool should_throw) {
  bool did_throw = false;

  try {
    func();
  } catch (torrent::internal_error& e) {
    did_throw = true;
  }

  return should_throw == did_throw;
}

void
tracker_list_features_test::test_request_safeguard() {
  TRACKER_SETUP();
  TRACKER_INSERT(0, tracker_1);
  TRACKER_INSERT(0, tracker_2);
  TRACKER_INSERT(0, tracker_3);
  TRACKER_INSERT(0, tracker_foo);

  for (unsigned int i = 0; i < 9; i++) {
    CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_state, &tracker_list, tracker_1, 1), false));
    CPPUNIT_ASSERT(tracker_1->trigger_success());
    CPPUNIT_ASSERT(tracker_1->success_counter() == (i + 1));
  }

  CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_state, &tracker_list, tracker_1, 1), true));
  CPPUNIT_ASSERT(tracker_1->trigger_success());
  
  torrent::cachedTime += rak::timer::from_seconds(1000);

  for (unsigned int i = 0; i < 9; i++) {
    CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_state, &tracker_list, tracker_foo, 1), false));
    CPPUNIT_ASSERT(tracker_foo->trigger_success());
    CPPUNIT_ASSERT(tracker_foo->success_counter() == (i + 1));
    CPPUNIT_ASSERT(tracker_foo->is_usable());
  }

  CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_state, &tracker_list, tracker_foo, 1), true));
  CPPUNIT_ASSERT(tracker_foo->trigger_success());
  
  for (unsigned int i = 0; i < 40; i++) {
    CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_state, &tracker_list, tracker_2, 1), false));
    CPPUNIT_ASSERT(tracker_2->trigger_success());
    CPPUNIT_ASSERT(tracker_2->success_counter() == (i + 1));

    torrent::cachedTime += rak::timer::from_seconds(1);
  }
  
  for (unsigned int i = 0; i < 17; i++) {
    CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_state, &tracker_list, tracker_3, 1), false));
    CPPUNIT_ASSERT(tracker_3->trigger_success());
    CPPUNIT_ASSERT(tracker_3->success_counter() == (i + 1));

    if (i % 2)
      torrent::cachedTime += rak::timer::from_seconds(1);
  }
  
  CPPUNIT_ASSERT(verify_did_internal_error(std::bind(&torrent::TrackerList::send_state, &tracker_list, tracker_3, 1), true));
  CPPUNIT_ASSERT(tracker_3->trigger_success());
}
