#include "config.h"

#include <functional>

#include "test/torrent/test_tracker_controller.h"
#include "test/torrent/test_tracker_list.h"
#include "test/helpers/test_main_thread.h"
#include "torrent/utils/log.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestTrackerController);

bool
test_tracker_value_in_range(uint32_t value, int32_t min, uint32_t max) {
  uint32_t range_min = min < 0 ? 0 : min;

  if (value < range_min || value > max) {
    lt_log_print(torrent::LOG_TRACKER_EVENTS, "test_tracker_value_in_range: %u not in range [%u, %u]", value, range_min, max);
    return false;
  }

  return true;
}

void
test_tracker_step_time(TestFixtureWithMainAndTrackerThread* fixture, int32_t seconds) {
  fixture->m_main_thread->test_add_cached_time(std::chrono::seconds(seconds));
  std::this_thread::sleep_for(50ms);

  fixture->m_main_thread->test_process_events_without_cached_time();
  std::this_thread::sleep_for(50ms);
}

bool
test_goto_next_timeout(TestFixtureWithMainAndTrackerThread* fixture,
                       torrent::TrackerController* tracker_controller,
                       uint32_t assumed_timeout,
                       bool is_scrape) {

  uint32_t next_timeout = tracker_controller->is_timeout_queued() ? tracker_controller->seconds_to_next_timeout() : ~uint32_t();
  uint32_t next_scrape  = tracker_controller->is_scrape_queued()  ? tracker_controller->seconds_to_next_scrape()  : ~uint32_t();

  bool result = true;
  std::string message;

  if (next_timeout == next_scrape && next_timeout == ~uint32_t()) {
    message = "(no timeout or scrape queued) ";
    result = false;
  }

  if (next_timeout < next_scrape) {
    if (is_scrape) {
      message += "n<s(t" + std::to_string(next_timeout) + "<s" + std::to_string(next_scrape) + ") ";
      result = false;
    }

    if (!test_tracker_value_in_range(next_timeout, (int32_t)assumed_timeout - 2, assumed_timeout + 2)) {
      message += "n<s(" + std::to_string(assumed_timeout) + "!=" + std::to_string(next_timeout) + ") ";
      result = false;
    }
  } else if (next_scrape < next_timeout) {
    if (!is_scrape) {
      message += "s<t(s" + std::to_string(next_scrape) + "<t" + std::to_string(next_timeout) + ") ";
      result = false;
    }

    if (!test_tracker_value_in_range(next_scrape, (int32_t)assumed_timeout - 2, assumed_timeout + 2)) {
      message += "s<t(" + std::to_string(assumed_timeout) + "!=" + std::to_string(next_scrape) + ") ";
      result = false;
    }
  }

  if (!result) {
    lt_log_print(torrent::LOG_TRACKER_EVENTS, "test_goto_next_timeout: %s", message.c_str());
    return false;
  }

  test_tracker_step_time(fixture, is_scrape ? (next_scrape + 1) : (next_timeout + 1));
  return true;
}

void
TestTrackerController::test_basic() {
  torrent::TrackerController tracker_controller(NULL);

  CPPUNIT_ASSERT(tracker_controller.flags() == 0);
  CPPUNIT_ASSERT(!tracker_controller.is_active());
  CPPUNIT_ASSERT(!tracker_controller.is_requesting());
}

void
TestTrackerController::test_enable() {
  TRACKER_CONTROLLER_SETUP();

  tracker_controller.enable();

  CPPUNIT_ASSERT(tracker_controller.is_active());
  CPPUNIT_ASSERT(!tracker_controller.is_requesting());
}

void
TestTrackerController::test_requesting() {
  TRACKER_CONTROLLER_SETUP();

  tracker_controller.enable();
  tracker_controller.start_requesting();

  CPPUNIT_ASSERT(tracker_controller.is_active());
  CPPUNIT_ASSERT(tracker_controller.is_requesting());

  tracker_controller.stop_requesting();

  CPPUNIT_ASSERT(tracker_controller.is_active());
  CPPUNIT_ASSERT(!tracker_controller.is_requesting());
}

void
TestTrackerController::test_timeout() {
  TRACKER_CONTROLLER_SETUP();
  TRACKER_INSERT(0, tracker_0_0);

  CPPUNIT_ASSERT(enabled_counter == 1);
  CPPUNIT_ASSERT(!tracker_0_0.is_busy());
  CPPUNIT_ASSERT(!tracker_controller.is_timeout_queued());

  tracker_controller.enable();

  m_main_thread->test_process_events_without_cached_time();

  CPPUNIT_ASSERT(timeout_counter == 1);
  TEST_SINGLE_END(0, 0);
}

void
TestTrackerController::test_single_success() {
  TEST_SINGLE_BEGIN();

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());

  CPPUNIT_ASSERT(success_counter == 1 && failure_counter == 0);

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_COMPLETED);
  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_failure());

  TEST_SINGLE_END(1, 1);
}

#define TEST_SINGLE_FAILURE_TIMEOUT(test_interval)                      \
  lt_log_print(torrent::LOG_TRACKER_EVENTS, "test_single_failure: main_thread->cached_time:%" PRIu64, torrent::this_thread::cached_time().count()); \
                                                                        \
  std::this_thread::sleep_for(100ms);                                   \
  m_main_thread->test_process_events_without_cached_time();             \
                                                                        \
  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_failure());                \
  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == test_interval); \
                                                                        \
  m_main_thread->test_add_cached_time(std::chrono::seconds(test_interval + 1));

void
TestTrackerController::test_single_failure() {
  m_main_thread->test_set_cached_time(0s);

  TEST_SINGLE_BEGIN();

  // TOOD: Need separate test handler for ThreadTracker?
  // TODO: Add a 'wait-for-one-event-loop' function to threads?
  // TODO: Add test-specific ThreadTracker that allows us to trigger process events and cached_time.

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);

  TEST_SINGLE_FAILURE_TIMEOUT(5);
  TEST_SINGLE_FAILURE_TIMEOUT(10);
  TEST_SINGLE_FAILURE_TIMEOUT(20);
  TEST_SINGLE_FAILURE_TIMEOUT(40);
  TEST_SINGLE_FAILURE_TIMEOUT(80);
  TEST_SINGLE_FAILURE_TIMEOUT(160);
  TEST_SINGLE_FAILURE_TIMEOUT(299);
  TEST_SINGLE_FAILURE_TIMEOUT(299);

  // TODO: Test with cached_time not rounded to second.

  TEST_SINGLE_END(0, 4);
}

void
TestTrackerController::test_single_disable() {
  TEST_SINGLE_BEGIN();
  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);
  TEST_SINGLE_END(0, 0);
}

void
TestTrackerController::test_send_start() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(start);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);

  CPPUNIT_ASSERT(!tracker_controller.is_timeout_queued());

  CPPUNIT_ASSERT(tracker_0_0.is_busy());
  CPPUNIT_ASSERT(tracker_0_0_worker->requesting_state() == torrent::tracker::TrackerState::EVENT_STARTED);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::mask_send));

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() != 0);

  tracker_controller.send_start_event();
  tracker_controller.disable();

  CPPUNIT_ASSERT(!tracker_0_0.is_busy());

  TEST_SEND_SINGLE_END(1, 0);
}

void
TestTrackerController::test_send_stop_normal() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);

  CPPUNIT_ASSERT(tracker_controller.is_timeout_queued());
  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());

  tracker_controller.send_stop_event();
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == torrent::TrackerController::flag_send_stop);

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == 0);

  tracker_controller.send_stop_event();
  tracker_controller.disable();

  CPPUNIT_ASSERT(tracker_0_0.is_busy());
  tracker_0_0_worker->trigger_success();

  TEST_SEND_SINGLE_END(3, 0);
}

// send_stop during request and right after start, send stop failed.

void
TestTrackerController::test_send_completed_normal() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);

  CPPUNIT_ASSERT(tracker_controller.is_timeout_queued());
  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());

  tracker_controller.send_completed_event();
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == torrent::TrackerController::flag_send_completed);

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == 0);

  tracker_controller.send_completed_event();
  tracker_controller.disable();

  CPPUNIT_ASSERT(tracker_0_0.is_busy());
  tracker_0_0_worker->trigger_success();

  TEST_SEND_SINGLE_END(3, 0);
}

void
TestTrackerController::test_send_update_normal() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);

  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == torrent::TrackerController::flag_send_update);

  CPPUNIT_ASSERT(tracker_controller.is_timeout_queued());
  CPPUNIT_ASSERT(tracker_0_0.state().latest_event() == torrent::tracker::TrackerState::EVENT_NONE);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());

  TEST_SEND_SINGLE_END(1, 0);
}

void
TestTrackerController::test_send_update_failure() {
  m_main_thread->test_set_cached_time(0s);

  TEST_SINGLE_BEGIN();

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);

  tracker_controller.send_update_event();

  TEST_SINGLE_FAILURE_TIMEOUT(5);
  TEST_SINGLE_FAILURE_TIMEOUT(10);

  TEST_SINGLE_END(0, 2);
}

void
TestTrackerController::test_send_task_timeout() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  CPPUNIT_ASSERT(tracker_controller.is_timeout_queued());

  TEST_SEND_SINGLE_END(0, 0);
}

void
TestTrackerController::test_send_close_on_enable() {
  TRACKER_CONTROLLER_SETUP();

  TRACKER_INSERT(0, tracker_0);
  TRACKER_INSERT(0, tracker_1);
  TRACKER_INSERT(0, tracker_2);
  TRACKER_INSERT(0, tracker_3);

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);
  tracker_list.send_event(tracker_list.at(1), torrent::tracker::TrackerState::EVENT_STARTED);
  tracker_list.send_event(tracker_list.at(2), torrent::tracker::TrackerState::EVENT_STOPPED);
  tracker_list.send_event(tracker_list.at(3), torrent::tracker::TrackerState::EVENT_COMPLETED);

  tracker_controller.enable();

  CPPUNIT_ASSERT(!tracker_0.is_busy());
  CPPUNIT_ASSERT(!tracker_1.is_busy());
  CPPUNIT_ASSERT(!tracker_2.is_busy());
  CPPUNIT_ASSERT(tracker_3.is_busy());
}

void
TestTrackerController::test_multiple_success() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, tracker_0_0.state().normal_interval()));
  TEST_MULTI3_IS_BUSY("10000", "10000");

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, tracker_0_0.state().normal_interval()));
  TEST_MULTI3_IS_BUSY("10000", "10000");

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());

  CPPUNIT_ASSERT(!tracker_list.has_active());
  CPPUNIT_ASSERT(tracker_0_0.state().success_counter() == 3);

  TEST_MULTIPLE_END(3, 0);
}

void
TestTrackerController::test_multiple_failure() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  auto tracker_0_1_worker = TrackerTest::test_worker(tracker_0_1);
  auto tracker_1_0_worker = TrackerTest::test_worker(tracker_1_0);
  auto tracker_2_0_worker = TrackerTest::test_worker(tracker_2_0);
  auto tracker_3_0_worker = TrackerTest::test_worker(tracker_3_0);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_failure());
  CPPUNIT_ASSERT(!tracker_controller.is_failure_mode());

  TEST_MULTI3_IS_BUSY("01000", "01000");
  CPPUNIT_ASSERT(tracker_0_1_worker->trigger_failure());
  TEST_MULTI3_IS_BUSY("00100", "00100");
  CPPUNIT_ASSERT(tracker_1_0_worker->trigger_success());

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, tracker_1_0.state().normal_interval()));

  TEST_MULTI3_IS_BUSY("10000", "10000");
  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_failure());
  TEST_MULTI3_IS_BUSY("01000", "01000");
  CPPUNIT_ASSERT(tracker_0_1_worker->trigger_failure());

  CPPUNIT_ASSERT(!tracker_controller.is_failure_mode());
  TEST_MULTI3_IS_BUSY("00100", "00100");
  CPPUNIT_ASSERT(tracker_1_0_worker->trigger_failure());
  CPPUNIT_ASSERT(tracker_controller.is_failure_mode());

  TEST_MULTI3_IS_BUSY("00010", "00010");
  CPPUNIT_ASSERT(tracker_2_0_worker->trigger_failure());
  TEST_MULTI3_IS_BUSY("00001", "00001");
  CPPUNIT_ASSERT(tracker_3_0_worker->trigger_failure());

  // Properly tests that we're going into timeouts... Disable remaining trackers.

  // for (int i = 0; i < 5; i++)
  //   std::cout << std::endl << i << ": "
  //             << tracker_list[i]->activity_time_last() << ' '
  //             << tracker_list[i]->success_time_last() << ' '
  //             << tracker_list[i]->failed_time_last() << std::endl;

  // Try inserting some delays in order to test the timers.

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 5));
  TEST_MULTI3_IS_BUSY("00100", "00100");
  CPPUNIT_ASSERT(tracker_1_0_worker->trigger_success());
  CPPUNIT_ASSERT(!tracker_controller.is_failure_mode());

  // CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, tracker_list[0].state().normal_interval()));
  // TEST_MULTI3_IS_BUSY("01000", "10000");
  // CPPUNIT_ASSERT(tracker_0_1_worker->trigger_success());

  CPPUNIT_ASSERT(TrackerTest::count_active(&tracker_list) == 0);
  TEST_MULTIPLE_END(2, 7);
}

void
TestTrackerController::test_multiple_cycle() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  auto tracker_0_1_worker = TrackerTest::test_worker(tracker_0_1);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_failure());
  CPPUNIT_ASSERT(tracker_0_1_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_list.front() == tracker_0_1);

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, tracker_0_1.state().normal_interval()));

  TEST_MULTI3_IS_BUSY("01000", "10000");
  CPPUNIT_ASSERT(tracker_0_1_worker->trigger_success());

  CPPUNIT_ASSERT(TrackerTest::count_active(&tracker_list) == 0);
  TEST_MULTIPLE_END(2, 1);
}

void
TestTrackerController::test_multiple_cycle_second_group() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  auto tracker_0_1_worker = TrackerTest::test_worker(tracker_0_1);
  auto tracker_1_0_worker = TrackerTest::test_worker(tracker_1_0);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_failure());
  CPPUNIT_ASSERT(tracker_0_1_worker->trigger_failure());
  CPPUNIT_ASSERT(tracker_1_0_worker->trigger_success());

  CPPUNIT_ASSERT(tracker_list.at(0) == tracker_0_0);
  CPPUNIT_ASSERT(tracker_list.at(1) == tracker_0_1);
  CPPUNIT_ASSERT(tracker_list.at(2) == tracker_1_0);
  CPPUNIT_ASSERT(tracker_list.at(3) == tracker_2_0);
  CPPUNIT_ASSERT(tracker_list.at(4) == tracker_3_0);

  TEST_MULTIPLE_END(1, 2);
}

void
TestTrackerController::test_multiple_send_stop() {
  TEST_MULTI3_BEGIN();

  auto tracker_0_1_worker = TrackerTest::test_worker(tracker_0_1);
  auto tracker_2_0_worker = TrackerTest::test_worker(tracker_2_0);
  auto tracker_3_0_worker = TrackerTest::test_worker(tracker_3_0);

  tracker_list.send_event(tracker_list.at(1), torrent::tracker::TrackerState::EVENT_NONE);
  tracker_list.send_event(tracker_list.at(3), torrent::tracker::TrackerState::EVENT_NONE);
  tracker_list.send_event(tracker_list.at(4), torrent::tracker::TrackerState::EVENT_NONE);

  CPPUNIT_ASSERT(tracker_0_1_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_2_0_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_3_0_worker->trigger_success());
  CPPUNIT_ASSERT(TrackerTest::count_active(&tracker_list) == 0);

  tracker_controller.send_stop_event();
  CPPUNIT_ASSERT(TrackerTest::count_active(&tracker_list) == 3);

  TEST_MULTI3_IS_BUSY("01011", "10011");
  CPPUNIT_ASSERT(tracker_0_1_worker->trigger_success());
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) != torrent::TrackerController::flag_send_stop);
  CPPUNIT_ASSERT(tracker_2_0_worker->trigger_success());
  CPPUNIT_ASSERT(tracker_3_0_worker->trigger_success());

  CPPUNIT_ASSERT(TrackerTest::count_active(&tracker_list) == 0);

  tracker_controller.send_stop_event();
  tracker_controller.disable();
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == torrent::TrackerController::flag_send_stop);
  TEST_MULTI3_IS_BUSY("01011", "10011");

  tracker_controller.enable(torrent::TrackerController::enable_dont_reset_stats);
  TEST_MULTI3_IS_BUSY("00000", "00000");
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == 0);

  tracker_controller.send_stop_event();
  TEST_MULTI3_IS_BUSY("01011", "10011");

  tracker_controller.disable();
  tracker_controller.enable();
  TEST_MULTI3_IS_BUSY("00000", "00000");
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == 0);

  tracker_controller.send_stop_event();
  TEST_MULTI3_IS_BUSY("00000", "00000");

  // Test also that after closing the tracker controller, and
  // reopening it a 'send stop' event causes no tracker to be busy.

  TEST_MULTIPLE_END(6, 0);
}

void
TestTrackerController::test_multiple_send_update() {
  TEST_MULTI3_BEGIN();

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  auto tracker_0_1_worker = TrackerTest::test_worker(tracker_0_1);

  tracker_controller.send_update_event();
  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 0));
  TEST_MULTI3_IS_BUSY("10000", "10000");

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());

  tracker_0_0_worker->set_failed(1, torrent::this_thread::cached_seconds().count());

  tracker_controller.send_update_event();
  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 0));
  TEST_MULTI3_IS_BUSY("01000", "01000");

  CPPUNIT_ASSERT(tracker_0_1_worker->trigger_failure());

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 0));
  TEST_MULTI3_IS_BUSY("00100", "00100");

  TEST_MULTIPLE_END(2, 0);
}

// Test timeout called, no usable trackers at all which leads to
// disabling the timeout.
//
// The enable/disable tracker functions will poke the tracker
// controller, ensuring the tast timeout gets re-started.
void
TestTrackerController::test_timeout_lacking_usable() {
  TEST_MULTI3_BEGIN();

  std::for_each(tracker_list.begin(), tracker_list.end(), std::mem_fn(&torrent::tracker::Tracker::disable));
  CPPUNIT_ASSERT(tracker_controller.is_timeout_queued());

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 0));

  TEST_MULTI3_IS_BUSY("00000", "00000");
  CPPUNIT_ASSERT(!tracker_controller.is_timeout_queued());

  tracker_1_0.enable();

  CPPUNIT_ASSERT(tracker_controller.is_timeout_queued());
  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 0));

  TEST_MULTI3_IS_BUSY("00100", "00100");

  CPPUNIT_ASSERT(!tracker_controller.is_timeout_queued());
  tracker_3_0.enable();
  CPPUNIT_ASSERT(!tracker_controller.is_timeout_queued());

  CPPUNIT_ASSERT(enabled_counter == 5 + 2 && disabled_counter == 5);
  TEST_MULTIPLE_END(0, 0);
}

void
TestTrackerController::test_disable_tracker() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  CPPUNIT_ASSERT(tracker_controller.is_timeout_queued());
  CPPUNIT_ASSERT(tracker_0_0.is_busy());

  tracker_0_0.disable();

  CPPUNIT_ASSERT(!tracker_0_0.is_busy());
  CPPUNIT_ASSERT(tracker_controller.is_timeout_queued());

  TEST_SINGLE_END(0, 0);
}

void
TestTrackerController::test_new_peers() {
  TRACKER_CONTROLLER_SETUP();

  TRACKER_INSERT(0, tracker_0_0);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);

  tracker_list.send_event(tracker_list.at(0), torrent::tracker::TrackerState::EVENT_NONE);

  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success(10));
  CPPUNIT_ASSERT(tracker_0_0.state().latest_new_peers() == 10);

  tracker_controller.enable();

  CPPUNIT_ASSERT(test_goto_next_timeout(this, &tracker_controller, 0));
  CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success(20));
  CPPUNIT_ASSERT(tracker_0_0.state().latest_new_peers() == 20);
}

// Add new function for finding the first tracker that will time out,
// e.g. both with failure mode and normal rerequesting.


// Make sure that adding a new tracker to a tracker list with no
// usable tracker restarts the tracker requests.

// Test timeout called, usable trackers exist but could not be called
// at this moment, thus leading to no active trackers. (Rather, clean
// up the 'is_usable()' functions, and make it a new function that is
// independent of enabled, or is itself manipulating/dependent on
// enabled)

// Add checks to ensure we don't prematurely do timeout on a tracker
// after disabling the lead one.


// Make sure that we always remain with timeout, even if we send a
// request that somehow doesn't actually activate any
// trackers. e.g. check all send_tracker_itr uses.


// Add test for promiscious mode while with a single tracker?



// Test send_* with controller not enabled.

// Test cleanup after disable.
// Test cleanup of timers at disable (and empty timers at enable).

// Test trying to send_start twice, etc.

// Test send_start promiscious...
//   - Make sure we check that no timer is inserted while still having active trackers.
//   - Calculate the next timeout according to a list of in-use trackers, with the first timeout as the interval.

// Test clearing of recv/failed counter on trackers.

