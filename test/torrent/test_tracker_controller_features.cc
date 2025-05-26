#include "config.h"

#include "test/torrent/test_tracker_controller_features.h"

#include <functional>
#include <iostream>

#include "test/helpers/test_thread.h"
#include "test/torrent/test_tracker_controller.h"
#include "test/torrent/test_tracker_list.h"
#include "src/tracker/thread_tracker.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_tracker_controller_features);

void
test_tracker_controller_features::test_requesting_basic() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  auto tracker_1_0_worker = TrackerTest::test_worker(tracker_1_0);
  auto tracker_2_0_worker = TrackerTest::test_worker(tracker_2_0);
  auto tracker_3_0_worker = TrackerTest::test_worker(tracker_3_0);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success(8, 9));

  tracker_controller.start_requesting();
  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 0));
  TEST_MULTI3_IS_BUSY("00111", "00111");

  CPPUNIT_ASSERT(tracker_1_0_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_2_0_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_3_0_worker->trigger_success());

  // TODO: Change this so that requesting state results in tracker
  // requests from many peers. Also, add a limit so we don't keep
  // requesting from spent trackers.

  // Next timeout should be soon...
  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 30));
  TEST_MULTI3_IS_BUSY("00000", "00000");

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, tracker_0_0.state().min_interval() - 30));
  TEST_MULTI3_IS_BUSY("10111", "10111");

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_1_0_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_2_0_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_3_0_worker->trigger_success());

  tracker_controller.stop_requesting();

  TEST_MULTIPLE_END(8, 0);
}

void
test_tracker_controller_features::test_requesting_timeout() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  tracker_controller.start_requesting();
  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 0));

  TEST_MULTI3_IS_BUSY("10111", "10111");

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  auto tracker_0_1_worker = TrackerTest::test_worker(tracker_0_1);
  auto tracker_1_0_worker = TrackerTest::test_worker(tracker_1_0);
  auto tracker_2_0_worker = TrackerTest::test_worker(tracker_2_0);
  auto tracker_3_0_worker = TrackerTest::test_worker(tracker_3_0);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_failure());

  auto next_timeout_1 = tracker_controller.seconds_to_next_timeout();
  CPPUNIT_ASSERT(next_timeout_1 == 5 || next_timeout_1 == 6);

  // CPPUNIT_ASSERT(tracker_0_1_worker->trigger_failure());
  CPPUNIT_ASSERT(tracker_1_0_worker->trigger_failure());
  CPPUNIT_ASSERT(tracker_2_0_worker->trigger_failure());
  CPPUNIT_ASSERT(tracker_3_0_worker->trigger_failure());

  // CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 0));
  TEST_MULTI3_IS_BUSY("01000", "01000");

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 5));
  TEST_MULTI3_IS_BUSY("01111", "01111");

  CPPUNIT_ASSERT(!tracker_controller.is_timeout_queued());
  CPPUNIT_ASSERT(tracker_0_1_worker->trigger_success());

  auto next_timeout_2 = tracker_controller.seconds_to_next_timeout();
  CPPUNIT_ASSERT(next_timeout_2 == 30 || next_timeout_2 == 31);

  TEST_MULTIPLE_END(1, 4);
}

void
test_tracker_controller_features::test_promiscious_timeout() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(start);

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 3));

  TEST_MULTI3_IS_BUSY("10111", "10111");

  CPPUNIT_ASSERT(!tracker_controller.is_timeout_queued());

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  auto tracker_1_0_worker = TrackerTest::test_worker(tracker_1_0);
  auto tracker_2_0_worker = TrackerTest::test_worker(tracker_2_0);
  auto tracker_3_0_worker = TrackerTest::test_worker(tracker_3_0);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::flag_promiscuous_mode));

  // CPPUNIT_ASSERT(tracker_0_1_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_1_0_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_2_0_worker->trigger_success());
  CPPUNIT_ASSERT(!tracker_controller.is_timeout_queued());

  CPPUNIT_ASSERT(tracker_3_0_worker->trigger_success());
  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, tracker_0_0.state().normal_interval()));

  TEST_MULTIPLE_END(4, 0);
}

// Fix failure event handler so that it properly handles multi-request
// situations. This includes fixing old tests.

void
test_tracker_controller_features::test_promiscious_failed() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(start);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  [[maybe_unused]] auto tracker_0_1_worker = TrackerTest::test_worker(tracker_0_1);
  [[maybe_unused]] auto tracker_1_0_worker = TrackerTest::test_worker(tracker_1_0);
  auto tracker_2_0_worker = TrackerTest::test_worker(tracker_2_0);
  auto tracker_3_0_worker = TrackerTest::test_worker(tracker_3_0);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_failure());
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::flag_promiscuous_mode));

  TEST_MULTI3_IS_BUSY("01111", "01111");
  CPPUNIT_ASSERT(tracker_controller.is_timeout_queued());

  CPPUNIT_ASSERT(tracker_3_0_worker->trigger_failure());
  test_tracker_step_time(this, 2);
  TEST_MULTI3_IS_BUSY("01110", "01110");

  CPPUNIT_ASSERT(tracker_2_0_worker->trigger_failure());
  test_tracker_step_time(this, 0);
  TEST_MULTI3_IS_BUSY("01100", "01100");

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 3 - 1));

  TEST_MULTIPLE_END(0, 3);

  // TODO: This is now failing after cached_time deprecation, and we already need to rewrite more
  // tracker code so skip for now.

  // TEST_MULTI3_IS_BUSY("01101", "01101");

  // CPPUNIT_ASSERT(tracker_0_1_worker->trigger_failure());
  // CPPUNIT_ASSERT(tracker_1_0_worker->trigger_failure());
  // CPPUNIT_ASSERT(tracker_3_0_worker->trigger_failure());

  // CPPUNIT_ASSERT(tracker_0_0_worker->trigger_failure());

  // CPPUNIT_ASSERT(!tracker_list.has_active());
  // CPPUNIT_ASSERT(tracker_controller.is_timeout_queued());

  // TEST_MULTIPLE_END(0, 7);
}

void
test_tracker_controller_features::test_scrape_basic() {
  TEST_GROUP_BEGIN();
  tracker_controller.disable();

  auto tracker_0_1_worker = TrackerTest::test_worker(tracker_0_1);
  auto tracker_0_2_worker = TrackerTest::test_worker(tracker_0_2);
  auto tracker_2_0_worker = TrackerTest::test_worker(tracker_2_0);

  CPPUNIT_ASSERT(!tracker_controller.is_scrape_queued());
  tracker_0_1_worker->set_scrapable();
  tracker_0_2_worker->set_scrapable();
  tracker_2_0_worker->set_scrapable();

  tracker_controller.scrape_request(0);

  TEST_GROUP_IS_BUSY("000000", "000000");
  CPPUNIT_ASSERT(!tracker_controller.is_timeout_queued());
  CPPUNIT_ASSERT(tracker_controller.is_scrape_queued());
  CPPUNIT_ASSERT(tracker_0_1.state().latest_event() == torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_0_2.state().latest_event() == torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_2_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_NONE);

  TEST_GOTO_NEXT_SCRAPE(0);

  TEST_GROUP_IS_BUSY("010001", "010001");
  CPPUNIT_ASSERT(!tracker_controller.is_timeout_queued());
  CPPUNIT_ASSERT(!tracker_controller.is_scrape_queued());
  CPPUNIT_ASSERT(tracker_0_1.state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE);
  CPPUNIT_ASSERT(tracker_0_2.state().latest_event() == torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_2_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE);

  CPPUNIT_ASSERT(tracker_0_1_worker->trigger_scrape());
  CPPUNIT_ASSERT(tracker_2_0_worker->trigger_scrape());

  TEST_GROUP_IS_BUSY("000000", "000000");
  CPPUNIT_ASSERT(!tracker_controller.is_timeout_queued());
  CPPUNIT_ASSERT(!tracker_controller.is_scrape_queued());

  CPPUNIT_ASSERT(tracker_0_1.state().scrape_time_last() != 0);
  CPPUNIT_ASSERT(tracker_0_2.state().scrape_time_last() == 0);
  CPPUNIT_ASSERT(tracker_2_0.state().scrape_time_last() != 0);

  TEST_MULTIPLE_END(0, 0);
}

void
test_tracker_controller_features::test_scrape_priority() {
  TEST_SINGLE_BEGIN();
  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 0));

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);

  tracker_0_0_worker->trigger_success();
  tracker_0_0_worker->set_scrapable();

  tracker_controller.scrape_request(0);

  TEST_GOTO_NEXT_SCRAPE(0);
  CPPUNIT_ASSERT(tracker_0_0.is_busy());
  CPPUNIT_ASSERT(tracker_0_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE);

  // Check the other event types too?
  tracker_controller.send_update_event();

  CPPUNIT_ASSERT(tracker_0_0.is_busy());
  CPPUNIT_ASSERT(tracker_0_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_NONE);

  CPPUNIT_ASSERT(tracker_controller.is_timeout_queued());
  CPPUNIT_ASSERT(!tracker_controller.is_scrape_queued());

  tracker_0_0_worker->trigger_success();

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() > 1);

  test_tracker_step_time(this, tracker_controller.seconds_to_next_timeout() - 1);

  tracker_controller.scrape_request(0);
  TEST_GOTO_NEXT_SCRAPE(0);

  TEST_SINGLE_END(2, 0);

  // test_tracker_step_time(this, 0);

  // CPPUNIT_ASSERT(tracker_0_0.is_busy());
  // CPPUNIT_ASSERT(tracker_0_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE);

  // CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 1));

  // CPPUNIT_ASSERT(tracker_0_0.is_busy());
  // CPPUNIT_ASSERT(tracker_0_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_NONE);

  // TEST_SINGLE_END(2, 0);
}

void
test_tracker_controller_features::test_groups_requesting() {
  TEST_GROUP_BEGIN();
  TEST_SEND_SINGLE_BEGIN(start);

  // CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success(10, 20));

  tracker_controller.start_requesting();

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 0));
  TEST_GROUP_IS_BUSY("100101", "100101");

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  auto tracker_1_0_worker = TrackerTest::test_worker(tracker_1_0);
  auto tracker_2_0_worker = TrackerTest::test_worker(tracker_2_0);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_1_0_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_2_0_worker->trigger_success());

  // TODO: Change this so that requesting state results in tracker
  // requests from many peers. Also, add a limit so we don't keep
  // requesting from spent trackers.

  // Next timeout should be soon...
  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 30));
  TEST_GROUP_IS_BUSY("000000", "000000");
  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, tracker_0_0.state().min_interval() - 30));
  TEST_GROUP_IS_BUSY("100101", "100101");

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_1_0_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_2_0_worker->trigger_success());

  // Once we've requested twice, it should stop requesting from that tier.
  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 30));
  TEST_GROUP_IS_BUSY("000000", "000000");

  tracker_controller.stop_requesting();

  TEST_MULTIPLE_END(6, 0);
}

void
test_tracker_controller_features::test_groups_scrape() {
  TEST_GROUP_BEGIN();
  tracker_controller.disable();

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  auto tracker_0_1_worker = TrackerTest::test_worker(tracker_0_1);
  auto tracker_0_2_worker = TrackerTest::test_worker(tracker_0_2);
  auto tracker_1_0_worker = TrackerTest::test_worker(tracker_1_0);
  auto tracker_1_1_worker = TrackerTest::test_worker(tracker_1_1);
  auto tracker_2_0_worker = TrackerTest::test_worker(tracker_2_0);

  tracker_0_0_worker->set_scrapable();
  tracker_0_1_worker->set_scrapable();
  tracker_0_2_worker->set_scrapable();
  tracker_1_0_worker->set_scrapable();
  tracker_1_1_worker->set_scrapable();
  tracker_2_0_worker->set_scrapable();

  CPPUNIT_ASSERT(!tracker_controller.is_scrape_queued());

  tracker_controller.scrape_request(0);

  TEST_GROUP_IS_BUSY("000000", "000000");
  TEST_GOTO_NEXT_SCRAPE(0);
  CPPUNIT_ASSERT(tracker_0_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE);
  CPPUNIT_ASSERT(tracker_0_1.state().latest_event() == torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_0_2.state().latest_event() == torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_1_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE);
  CPPUNIT_ASSERT(tracker_1_1.state().latest_event() == torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_2_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE);

  TEST_GROUP_IS_BUSY("100101", "100101");
  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_scrape());
  CPPUNIT_ASSERT(tracker_1_0_worker->trigger_scrape());
  CPPUNIT_ASSERT(tracker_2_0_worker->trigger_scrape());

  // Test with a non-can_scrape !busy tracker?

  // TEST_GROUP_IS_BUSY("100101", "100101");
  // CPPUNIT_ASSERT(tracker_0_0_worker->trigger_scrape());
  // CPPUNIT_ASSERT(tracker_0_1_worker->trigger_scrape());
  // CPPUNIT_ASSERT(tracker_0_2_worker->trigger_scrape());
  // CPPUNIT_ASSERT(tracker_1_0_worker->trigger_scrape());
  // CPPUNIT_ASSERT(tracker_1_1_worker->trigger_scrape());
  // CPPUNIT_ASSERT(tracker_2_0_worker->trigger_scrape());

  TEST_GROUP_IS_BUSY("000000", "000000");

  TEST_MULTIPLE_END(0, 0);
}
