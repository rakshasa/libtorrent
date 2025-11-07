#include "config.h"

#include "test/torrent/test_tracker_list.h"

#include "net/address_list.h"
#include "torrent/utils/uri_parser.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestTrackerList);

void
TestTrackerList::test_basic() {
  TRACKER_LIST_SETUP();
  TRACKER_INSERT(0, tracker_0);

  CPPUNIT_ASSERT(tracker_0 == tracker_list.at(0));

  CPPUNIT_ASSERT(std::distance(tracker_list.begin_group(0), tracker_list.end_group(0)) == 1);

  auto usable_itr = std::find_if(tracker_list.begin(), tracker_list.end(), std::mem_fn(&torrent::tracker::Tracker::is_usable));
  CPPUNIT_ASSERT(usable_itr != tracker_list.end());
}

void
TestTrackerList::test_enable() {
  TRACKER_LIST_SETUP();
  int enabled_counter = 0;
  int disabled_counter = 0;

  tracker_list.slot_tracker_enabled() = std::bind(&increment_value_void, &enabled_counter);
  tracker_list.slot_tracker_disabled() = std::bind(&increment_value_void, &disabled_counter);

  TRACKER_INSERT(0, tracker_0);
  TRACKER_INSERT(1, tracker_1);
  CPPUNIT_ASSERT(enabled_counter == 2 && disabled_counter == 0);

  tracker_0.enable(); tracker_1.enable();
  CPPUNIT_ASSERT(enabled_counter == 2 && disabled_counter == 0);

  tracker_0.disable(); tracker_1.enable();
  CPPUNIT_ASSERT(enabled_counter == 2 && disabled_counter == 1);

  tracker_1.disable(); tracker_0.disable();
  CPPUNIT_ASSERT(enabled_counter == 2 && disabled_counter == 2);

  tracker_0.enable(); tracker_1.enable();
  tracker_0.enable(); tracker_1.enable();
  CPPUNIT_ASSERT(enabled_counter == 4 && disabled_counter == 2);
}

void
TestTrackerList::test_close() {
  TRACKER_LIST_SETUP();

  TRACKER_INSERT(0, tracker_0);
  TRACKER_INSERT(0, tracker_1);
  TRACKER_INSERT(0, tracker_2);
  TRACKER_INSERT(0, tracker_3);

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);
  tracker_list.send_event(tracker_list.at(1), torrent::tracker::TrackerState::EVENT_STARTED);
  tracker_list.send_event(tracker_list.at(2), torrent::tracker::TrackerState::EVENT_STOPPED);
  tracker_list.send_event(tracker_list.at(3), torrent::tracker::TrackerState::EVENT_COMPLETED);

  CPPUNIT_ASSERT(tracker_0.is_busy());
  CPPUNIT_ASSERT(tracker_1.is_busy());
  CPPUNIT_ASSERT(tracker_2.is_busy());
  CPPUNIT_ASSERT(tracker_3.is_busy());

  tracker_list.close_all_excluding((1 << torrent::tracker::TrackerState::EVENT_STARTED) | (1 << torrent::tracker::TrackerState::EVENT_STOPPED));

  CPPUNIT_ASSERT(!tracker_0.is_busy());
  CPPUNIT_ASSERT(tracker_1.is_busy());
  CPPUNIT_ASSERT(tracker_2.is_busy());
  CPPUNIT_ASSERT(!tracker_3.is_busy());

  tracker_list.close_all();

  CPPUNIT_ASSERT(!tracker_0.is_busy());
  CPPUNIT_ASSERT(!tracker_1.is_busy());
  CPPUNIT_ASSERT(!tracker_2.is_busy());
  CPPUNIT_ASSERT(!tracker_3.is_busy());

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);
  tracker_list.send_event(tracker_list.at(1), torrent::tracker::TrackerState::EVENT_STARTED);
  tracker_list.send_event(tracker_list.at(2), torrent::tracker::TrackerState::EVENT_STOPPED);
  tracker_list.send_event(tracker_list.at(3), torrent::tracker::TrackerState::EVENT_COMPLETED);

  tracker_list.close_all_excluding((1 << torrent::tracker::TrackerState::EVENT_NONE) | (1 << torrent::tracker::TrackerState::EVENT_COMPLETED));

  CPPUNIT_ASSERT(tracker_0.is_busy());
  CPPUNIT_ASSERT(!tracker_1.is_busy());
  CPPUNIT_ASSERT(!tracker_2.is_busy());
  CPPUNIT_ASSERT(tracker_3.is_busy());
}

// Test clear.

void
TestTrackerList::test_tracker_flags() {
  TRACKER_LIST_SETUP();

  tracker_list.insert(0, TrackerTest::new_tracker(&tracker_list, ""));
  tracker_list.insert(0, TrackerTest::new_tracker(&tracker_list, "", 0));
  tracker_list.insert(0, TrackerTest::new_tracker(&tracker_list, "", torrent::tracker::TrackerState::flag_enabled));
  tracker_list.insert(0, TrackerTest::new_tracker(&tracker_list, "", torrent::tracker::TrackerState::flag_extra_tracker));
  tracker_list.insert(0, TrackerTest::new_tracker(&tracker_list, "", torrent::tracker::TrackerState::flag_enabled | torrent::tracker::TrackerState::flag_extra_tracker));

  CPPUNIT_ASSERT((TrackerTest::test_flags(tracker_list.at(0)) & 0xf) == torrent::tracker::TrackerState::flag_enabled);
  CPPUNIT_ASSERT((TrackerTest::test_flags(tracker_list.at(1)) & 0xf) == 0);
  CPPUNIT_ASSERT((TrackerTest::test_flags(tracker_list.at(2)) & 0xf) == torrent::tracker::TrackerState::flag_enabled);
  CPPUNIT_ASSERT((TrackerTest::test_flags(tracker_list.at(3)) & 0xf) == torrent::tracker::TrackerState::flag_extra_tracker);
  CPPUNIT_ASSERT((TrackerTest::test_flags(tracker_list.at(4)) & 0xf) == (torrent::tracker::TrackerState::flag_enabled | torrent::tracker::TrackerState::flag_extra_tracker));
}

void
TestTrackerList::test_find_url() {
  TRACKER_LIST_SETUP();

  tracker_list.insert(0, TrackerTest::new_tracker(&tracker_list, "http://1"));
  tracker_list.insert(0, TrackerTest::new_tracker(&tracker_list, "http://2"));
  tracker_list.insert(1, TrackerTest::new_tracker(&tracker_list, "http://3"));

  CPPUNIT_ASSERT(tracker_list.find_url("http://") == tracker_list.end());

  CPPUNIT_ASSERT(tracker_list.find_url("http://1") != tracker_list.end());
  CPPUNIT_ASSERT(*tracker_list.find_url("http://1") == tracker_list.at(0));

  CPPUNIT_ASSERT(tracker_list.find_url("http://2") != tracker_list.end());
  CPPUNIT_ASSERT(*tracker_list.find_url("http://2") == tracker_list.at(1));

  CPPUNIT_ASSERT(tracker_list.find_url("http://3") != tracker_list.end());
  CPPUNIT_ASSERT(*tracker_list.find_url("http://3") == tracker_list.at(2));
}

void
TestTrackerList::test_can_scrape() {
  TRACKER_LIST_SETUP();

  tracker_list.insert_url(0, "http://example.com/announce");
  CPPUNIT_ASSERT(tracker_list.back().is_scrapable());
  CPPUNIT_ASSERT(torrent::utils::uri_generate_scrape_url(tracker_list.back().url()) ==
                 "http://example.com/scrape");

  tracker_list.insert_url(0, "http://example.com/x/announce");
  CPPUNIT_ASSERT(tracker_list.back().is_scrapable());
  CPPUNIT_ASSERT(torrent::utils::uri_generate_scrape_url(tracker_list.back().url()) ==
                 "http://example.com/x/scrape");

  tracker_list.insert_url(0, "http://example.com/announce.php");
  CPPUNIT_ASSERT(tracker_list.back().is_scrapable());
  CPPUNIT_ASSERT(torrent::utils::uri_generate_scrape_url(tracker_list.back().url()) ==
                 "http://example.com/scrape.php");

  tracker_list.insert_url(0, "http://example.com/a");
  CPPUNIT_ASSERT(!tracker_list.back().is_scrapable());

  tracker_list.insert_url(0, "http://example.com/announce?x2%0644");
  CPPUNIT_ASSERT(tracker_list.back().is_scrapable());
  CPPUNIT_ASSERT(torrent::utils::uri_generate_scrape_url(tracker_list.back().url()) ==
                 "http://example.com/scrape?x2%0644");

  tracker_list.insert_url(0, "http://example.com/announce?x=2/4");
  CPPUNIT_ASSERT(!tracker_list.back().is_scrapable());

  tracker_list.insert_url(0, "http://example.com/x%064announce");
  CPPUNIT_ASSERT(!tracker_list.back().is_scrapable());
}

void
TestTrackerList::test_single_success() {
  TRACKER_LIST_SETUP();
  TRACKER_INSERT(0, tracker_0);

  auto tracker_0_worker = TrackerTest::test_worker(tracker_0);

  CPPUNIT_ASSERT(!tracker_0.is_busy());
  CPPUNIT_ASSERT(!tracker_0.is_busy_not_scrape());
  CPPUNIT_ASSERT(!tracker_0_worker->is_open());
  CPPUNIT_ASSERT(tracker_0_worker->requesting_state() == -1);
  CPPUNIT_ASSERT(tracker_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_NONE);

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_STARTED);

  CPPUNIT_ASSERT(tracker_0.is_busy());
  CPPUNIT_ASSERT(tracker_0.is_busy_not_scrape());
  CPPUNIT_ASSERT(tracker_0_worker->is_open());
  CPPUNIT_ASSERT(tracker_0_worker->requesting_state() == torrent::tracker::TrackerState::EVENT_STARTED);
  CPPUNIT_ASSERT(tracker_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_STARTED);

  CPPUNIT_ASSERT(tracker_0_worker->trigger_success());

  CPPUNIT_ASSERT(!tracker_0.is_busy());
  CPPUNIT_ASSERT(!tracker_0.is_busy_not_scrape());
  CPPUNIT_ASSERT(!tracker_0_worker->is_open());
  CPPUNIT_ASSERT(tracker_0_worker->requesting_state() == -1);
  CPPUNIT_ASSERT(tracker_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_STARTED);

  CPPUNIT_ASSERT(success_counter == 1 && failure_counter == 0);
  CPPUNIT_ASSERT(tracker_0.state().success_counter() == 1);
  CPPUNIT_ASSERT(tracker_0.state().failed_counter() == 0);
}

void
TestTrackerList::test_single_failure() {
  TRACKER_LIST_SETUP();
  TRACKER_INSERT(0, tracker_0);

  auto tracker_0_worker = TrackerTest::test_worker(tracker_0);

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_0_worker->trigger_failure());

  CPPUNIT_ASSERT(!tracker_0.is_busy());
  CPPUNIT_ASSERT(!tracker_0_worker->is_open());
  CPPUNIT_ASSERT(tracker_0_worker->requesting_state() == -1);

  CPPUNIT_ASSERT(success_counter == 0 && failure_counter == 1);
  CPPUNIT_ASSERT(tracker_0.state().success_counter() == 0);
  CPPUNIT_ASSERT(tracker_0.state().failed_counter() == 1);

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_0_worker->trigger_success());

  CPPUNIT_ASSERT(success_counter == 1 && failure_counter == 1);
  CPPUNIT_ASSERT(tracker_0.state().success_counter() == 1);
  CPPUNIT_ASSERT(tracker_0.state().failed_counter() == 0);
}

void
TestTrackerList::test_single_closing() {
  TRACKER_LIST_SETUP();
  TRACKER_INSERT(0, tracker_0);

  auto tracker_0_worker = TrackerTest::test_worker(tracker_0);

  CPPUNIT_ASSERT(!tracker_0_worker->is_open());

  tracker_0_worker->set_close_on_done(false);
  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);

  CPPUNIT_ASSERT(tracker_0_worker->is_open());
  CPPUNIT_ASSERT(tracker_0_worker->trigger_success());

  CPPUNIT_ASSERT(!tracker_0.is_busy());
  CPPUNIT_ASSERT(tracker_0_worker->is_open());

  tracker_list.close_all();
  tracker_list.clear_stats();

  CPPUNIT_ASSERT(!tracker_0_worker->is_open());
  CPPUNIT_ASSERT(tracker_0.state().success_counter() == 0);
  CPPUNIT_ASSERT(tracker_0.state().failed_counter() == 0);
}

void
TestTrackerList::test_multiple_success() {
  TRACKER_LIST_SETUP();

  TRACKER_INSERT(0, tracker_0_0);
  TRACKER_INSERT(0, tracker_0_1);
  TRACKER_INSERT(1, tracker_1_0);
  TRACKER_INSERT(1, tracker_1_1);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  auto tracker_0_1_worker = TrackerTest::test_worker(tracker_0_1);
  auto tracker_1_1_worker = TrackerTest::test_worker(tracker_1_1);

  CPPUNIT_ASSERT(!tracker_0_0.is_busy());
  CPPUNIT_ASSERT(!tracker_0_1.is_busy());
  CPPUNIT_ASSERT(!tracker_1_0.is_busy());
  CPPUNIT_ASSERT(!tracker_1_1.is_busy());

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);

  CPPUNIT_ASSERT(tracker_0_0.is_busy());
  CPPUNIT_ASSERT(!tracker_0_1.is_busy());
  CPPUNIT_ASSERT(!tracker_1_0.is_busy());
  CPPUNIT_ASSERT(!tracker_1_1.is_busy());

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());

  CPPUNIT_ASSERT(!tracker_0_0.is_busy());
  CPPUNIT_ASSERT(!tracker_0_1.is_busy());
  CPPUNIT_ASSERT(!tracker_1_0.is_busy());
  CPPUNIT_ASSERT(!tracker_1_1.is_busy());

  tracker_list.send_event(tracker_list.at(1), torrent::tracker::TrackerState::EVENT_NONE);
  tracker_list.send_event(tracker_list.at(3), torrent::tracker::TrackerState::EVENT_NONE);

  CPPUNIT_ASSERT(!tracker_0_0.is_busy());
  CPPUNIT_ASSERT(tracker_0_1.is_busy());
  CPPUNIT_ASSERT(!tracker_1_0.is_busy());
  CPPUNIT_ASSERT(tracker_1_1.is_busy());

  CPPUNIT_ASSERT(tracker_1_1_worker->trigger_success());

  CPPUNIT_ASSERT(!tracker_0_0.is_busy());
  CPPUNIT_ASSERT(tracker_0_1.is_busy());
  CPPUNIT_ASSERT(!tracker_1_0.is_busy());
  CPPUNIT_ASSERT(!tracker_1_1.is_busy());

  CPPUNIT_ASSERT(tracker_0_1_worker->trigger_success());

  CPPUNIT_ASSERT(!tracker_0_0.is_busy());
  CPPUNIT_ASSERT(!tracker_0_1.is_busy());
  CPPUNIT_ASSERT(!tracker_1_0.is_busy());
  CPPUNIT_ASSERT(!tracker_1_1.is_busy());

  CPPUNIT_ASSERT(success_counter == 3 && failure_counter == 0);
}

void
TestTrackerList::test_scrape_success() {
  TRACKER_LIST_SETUP();
  TRACKER_INSERT(0, tracker_0);

  auto tracker_0_worker = TrackerTest::test_worker(tracker_0);

  tracker_0_worker->set_scrapable();
  tracker_list.send_scrape(tracker_0);

  CPPUNIT_ASSERT(tracker_0.is_busy());
  CPPUNIT_ASSERT(!tracker_0.is_busy_not_scrape());
  CPPUNIT_ASSERT(tracker_0_worker->is_open());
  CPPUNIT_ASSERT(tracker_0_worker->requesting_state() == torrent::tracker::TrackerState::EVENT_SCRAPE);
  CPPUNIT_ASSERT(tracker_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE);

  CPPUNIT_ASSERT(tracker_0_worker->trigger_scrape());

  CPPUNIT_ASSERT(!tracker_0.is_busy());
  CPPUNIT_ASSERT(!tracker_0.is_busy_not_scrape());
  CPPUNIT_ASSERT(!tracker_0_worker->is_open());
  CPPUNIT_ASSERT(tracker_0_worker->requesting_state() == -1);
  CPPUNIT_ASSERT(tracker_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE);

  CPPUNIT_ASSERT(success_counter == 0 && failure_counter == 0);
  CPPUNIT_ASSERT(scrape_success_counter == 1 && scrape_failure_counter == 0);
  CPPUNIT_ASSERT(tracker_0.state().success_counter() == 0);
  CPPUNIT_ASSERT(tracker_0.state().failed_counter() == 0);
  CPPUNIT_ASSERT(tracker_0.state().scrape_counter() == 1);
}

void
TestTrackerList::test_scrape_failure() {
  TRACKER_LIST_SETUP();
  TRACKER_INSERT(0, tracker_0);

  auto tracker_0_worker = TrackerTest::test_worker(tracker_0);

  tracker_0_worker->set_scrapable();
  tracker_list.send_scrape(tracker_0);

  CPPUNIT_ASSERT(tracker_0_worker->trigger_failure());

  CPPUNIT_ASSERT(!tracker_0.is_busy());
  CPPUNIT_ASSERT(!tracker_0_worker->is_open());
  CPPUNIT_ASSERT(tracker_0_worker->requesting_state() == -1);
  CPPUNIT_ASSERT(tracker_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE);

  CPPUNIT_ASSERT(success_counter == 0 && failure_counter == 0);
  CPPUNIT_ASSERT(scrape_success_counter == 0 && scrape_failure_counter == 1);
  CPPUNIT_ASSERT(tracker_0.state().success_counter() == 0);
  CPPUNIT_ASSERT(tracker_0.state().failed_counter() == 0);
  CPPUNIT_ASSERT(tracker_0.state().scrape_counter() == 0);
}

bool
check_has_active_in_group(const torrent::TrackerList* tracker_list, const char* states, bool scrape) {
  int group = 0;

  while (*states != '\0') {
    bool result = scrape ?
      tracker_list->has_active_in_group(group++) :
      tracker_list->has_active_not_scrape_in_group(group++);

    if ((*states == '1' && !result) ||
        (*states == '0' && result))
      return false;

    states++;
  }

  return true;
}

void
TestTrackerList::test_has_active() {
  TRACKER_LIST_SETUP();

  TRACKER_INSERT(0, tracker_0);
  TRACKER_INSERT(0, tracker_1);
  TRACKER_INSERT(1, tracker_2);
  TRACKER_INSERT(3, tracker_3);
  TRACKER_INSERT(4, tracker_4);

  // TODO: Test scrape...
  auto tracker_0_worker = TrackerTest::test_worker(tracker_0);
  auto tracker_1_worker = TrackerTest::test_worker(tracker_1);
  auto tracker_2_worker = TrackerTest::test_worker(tracker_2);
  auto tracker_3_worker = TrackerTest::test_worker(tracker_3);

  TEST_TRACKERS_IS_BUSY_5("00000", "00000");
  CPPUNIT_ASSERT(!tracker_list.has_active());
  CPPUNIT_ASSERT(!tracker_list.has_active_not_scrape());
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "000000", false));
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "000000", true));

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);
  TEST_TRACKERS_IS_BUSY_5("10000", "10000");
  CPPUNIT_ASSERT( tracker_list.has_active());
  CPPUNIT_ASSERT( tracker_list.has_active_not_scrape());
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "100000", false));
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "100000", true));

  CPPUNIT_ASSERT(tracker_0_worker->trigger_success());
  TEST_TRACKERS_IS_BUSY_5("00000", "00000");
  CPPUNIT_ASSERT(!tracker_list.has_active());
  CPPUNIT_ASSERT(!tracker_list.has_active_not_scrape());
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "000000", false));
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "000000", true));

  tracker_list.send_event(tracker_list.at(1), torrent::tracker::TrackerState::EVENT_NONE);
  tracker_list.send_event(tracker_list.at(3), torrent::tracker::TrackerState::EVENT_NONE);
  TEST_TRACKERS_IS_BUSY_5("01010", "01010");
  CPPUNIT_ASSERT( tracker_list.has_active());
  CPPUNIT_ASSERT( tracker_list.has_active_not_scrape());
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "100100", false));
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "100100", true));

  tracker_2_worker->set_scrapable();
  tracker_list.send_scrape(tracker_2);

  tracker_list.send_event(tracker_list.at(1), torrent::tracker::TrackerState::EVENT_NONE);
  tracker_list.send_event(tracker_list.at(3), torrent::tracker::TrackerState::EVENT_NONE);
  TEST_TRACKERS_IS_BUSY_5("01110", "01110");
  CPPUNIT_ASSERT( tracker_list.has_active());
  CPPUNIT_ASSERT( tracker_list.has_active_not_scrape());
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "100100", false));
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "110100", true));

  CPPUNIT_ASSERT(tracker_1_worker->trigger_success());
  TEST_TRACKERS_IS_BUSY_5("00110", "00110");
  CPPUNIT_ASSERT( tracker_list.has_active());
  CPPUNIT_ASSERT( tracker_list.has_active_not_scrape());
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "000100", false));
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "010100", true));

  CPPUNIT_ASSERT(tracker_2_worker->trigger_scrape());
  CPPUNIT_ASSERT(tracker_3_worker->trigger_success());
  TEST_TRACKERS_IS_BUSY_5("00000", "00000");
  CPPUNIT_ASSERT(!tracker_list.has_active());
  CPPUNIT_ASSERT(!tracker_list.has_active_not_scrape());
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "000000", false));
  CPPUNIT_ASSERT(check_has_active_in_group(&tracker_list, "000000", true));
}
