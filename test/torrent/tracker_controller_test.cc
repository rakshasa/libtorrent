#include "config.h"

#include <iostream>
#include <torrent/tracker_controller.h>

#include "rak/priority_queue_default.h"

#include "globals.h"
#include "tracker_list_test.h"
#include "tracker_controller_test.h"

namespace std { using namespace tr1; }

CPPUNIT_TEST_SUITE_REGISTRATION(tracker_controller_test);

#define TRACKER_SETUP()                                                 \
  torrent::TrackerList tracker_list;                                    \
  torrent::TrackerController tracker_controller(&tracker_list);         \
                                                                        \
  int success_counter = 0;                                              \
  int failure_counter = 0;                                              \
  int timeout_counter = 0;                                              \
  int enabled_counter = 0;                                              \
  int disabled_counter = 0;                                             \
                                                                        \
  tracker_controller.slot_success() = std::bind(&increment_value, &success_counter); \
  tracker_controller.slot_failure() = std::bind(&increment_value, &failure_counter); \
  tracker_controller.slot_timeout() = std::bind(&increment_value, &timeout_counter); \
  tracker_controller.slot_tracker_enabled() = std::bind(&increment_value, &enabled_counter); \
  tracker_controller.slot_tracker_disabled() = std::bind(&increment_value, &disabled_counter); \
                                                                        \
  tracker_list.slot_success() = std::bind(&torrent::TrackerController::receive_success, &tracker_controller, std::placeholders::_1, std::placeholders::_2); \
  tracker_list.slot_failure() = std::bind(&torrent::TrackerController::receive_failure, &tracker_controller, std::placeholders::_1, std::placeholders::_2); \
  tracker_list.slot_tracker_enabled()  = std::bind(&torrent::TrackerController::receive_tracker_enabled, &tracker_controller, std::placeholders::_1); \
  tracker_list.slot_tracker_disabled() = std::bind(&torrent::TrackerController::receive_tracker_disabled, &tracker_controller, std::placeholders::_1);

#define TRACKER_INSERT(group, name)                             \
  TrackerTest* name = new TrackerTest(&tracker_list, "");       \
  tracker_list.insert(group, name);

#define TEST_SINGLE_BEGIN()                                             \
  TRACKER_SETUP();                                                      \
  TRACKER_INSERT(0, tracker_0_0);                                       \
                                                                        \
  tracker_controller.enable();                                          \
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::mask_send)); \

#define TEST_SINGLE_END(succeeded, failed)            \
  tracker_controller.disable();                                         \
  CPPUNIT_ASSERT(!tracker_list.has_active());                           \
  CPPUNIT_ASSERT(success_counter == succeeded &&                        \
                 failure_counter == failure_counter);

#define TEST_SEND_SINGLE_BEGIN(event_name)                              \
  tracker_controller.send_##event_name##_event();                       \
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == \
                 torrent::TrackerController::flag_send_##event_name);   \
                                                                        \
  CPPUNIT_ASSERT(tracker_controller.is_active());                       \
  CPPUNIT_ASSERT(tracker_controller.tracker_list()->count_active() == 1);

#define TEST_SEND_SINGLE_END(succeeded, failed)                         \
  TEST_SINGLE_END(succeeded, failed)                                    \
  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);    \
  //CPPUNIT_ASSERT(tracker_controller.seconds_to_promicious_mode() != 0);

#define TEST_MULTI3_BEGIN()                                             \
  TRACKER_SETUP();                                                      \
  TRACKER_INSERT(0, tracker_0_0);                                       \
  TRACKER_INSERT(0, tracker_0_1);                                       \
  TRACKER_INSERT(1, tracker_1_0);                                       \
  TRACKER_INSERT(2, tracker_2_0);                                       \
  TRACKER_INSERT(3, tracker_3_0);                                       \
                                                                        \
  tracker_controller.enable();                                          \
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::mask_send)); \

#define TEST_MULTIPLE_END(succeeded, failed)                            \
  tracker_controller.disable();                                         \
  CPPUNIT_ASSERT(!tracker_list.has_active());                           \
  CPPUNIT_ASSERT(success_counter == succeeded &&                        \
                 failure_counter == failed);

#define TEST_IS_BUSY(tracker, state)                                    \
  CPPUNIT_ASSERT(state == '0' ||  tracker->is_busy());                  \
  CPPUNIT_ASSERT(state == '1' || !tracker->is_busy());

#define TEST_MULTI3_IS_BUSY(original, rearranged)         \
  TEST_IS_BUSY(tracker_0_0, original[0]);                 \
  TEST_IS_BUSY(tracker_0_1, original[1]);                 \
  TEST_IS_BUSY(tracker_1_0, original[2]);                 \
  TEST_IS_BUSY(tracker_2_0, original[3]);                 \
  TEST_IS_BUSY(tracker_3_0, original[4]);                 \
  TEST_IS_BUSY(tracker_list[0], rearranged[0]);           \
  TEST_IS_BUSY(tracker_list[1], rearranged[1]);           \
  TEST_IS_BUSY(tracker_list[2], rearranged[2]);           \
  TEST_IS_BUSY(tracker_list[3], rearranged[3]);           \
  TEST_IS_BUSY(tracker_list[4], rearranged[4]);

#define TEST_GOTO_NEXT_TIMEOUT(assumed_timeout)                         \
  CPPUNIT_ASSERT(tracker_controller.task_timeout()->is_queued());       \
  CPPUNIT_ASSERT(assumed_timeout == tracker_controller.seconds_to_next_timeout()); \
  torrent::cachedTime += rak::timer::from_seconds(tracker_controller.seconds_to_next_timeout()); \
  rak::priority_queue_perform(&torrent::taskScheduler, torrent::cachedTime);

#define TEST_GOTO_NEXT_SCRAPE(assumed_scrape)                           \
  CPPUNIT_ASSERT(tracker_controller.task_scrape()->is_queued());        \
  CPPUNIT_ASSERT(assumed_scrape == tracker_controller.seconds_to_next_scrape()); \
  torrent::cachedTime += rak::timer::from_seconds(tracker_controller.seconds_to_next_scrape()); \
  rak::priority_queue_perform(&torrent::taskScheduler, torrent::cachedTime);

void
tracker_controller_test::setUp() {
  torrent::cachedTime = rak::timer::current();
}

void
tracker_controller_test::tearDown() {
}

void
tracker_controller_test::test_basic() {
  torrent::TrackerController tracker_controller(NULL);

  CPPUNIT_ASSERT(tracker_controller.flags() == 0);
  CPPUNIT_ASSERT(!tracker_controller.is_active());
  CPPUNIT_ASSERT(!tracker_controller.is_requesting());
}

void
tracker_controller_test::test_enable() {
  torrent::TrackerList tracker_list;
  torrent::TrackerController tracker_controller(&tracker_list);

  tracker_controller.enable();

  CPPUNIT_ASSERT(tracker_controller.is_active());
  CPPUNIT_ASSERT(!tracker_controller.is_requesting());
}

void
tracker_controller_test::test_requesting() {
  torrent::TrackerList tracker_list;
  torrent::TrackerController tracker_controller(&tracker_list);

  tracker_controller.enable();
  tracker_controller.start_requesting();

  CPPUNIT_ASSERT(tracker_controller.is_active());
  CPPUNIT_ASSERT(tracker_controller.is_requesting());

  tracker_controller.stop_requesting();

  CPPUNIT_ASSERT(tracker_controller.is_active());
  CPPUNIT_ASSERT(!tracker_controller.is_requesting());
}

void
tracker_controller_test::test_timeout() {
  TRACKER_SETUP();
  TRACKER_INSERT(0, tracker_0_0);

  CPPUNIT_ASSERT(enabled_counter == 1);
  CPPUNIT_ASSERT(!tracker_0_0->is_busy());
  CPPUNIT_ASSERT(!tracker_controller.task_timeout()->is_queued());

  tracker_controller.enable();

  rak::priority_queue_perform(&torrent::taskScheduler, torrent::cachedTime);

  CPPUNIT_ASSERT(timeout_counter == 1);
  TEST_SINGLE_END(0, 0);
}

void
tracker_controller_test::test_single_success() {
  TEST_SINGLE_BEGIN();
  
  tracker_list.send_state_idx(0, 1);
  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  CPPUNIT_ASSERT(success_counter == 1 && failure_counter == 0);
  
  tracker_list.send_state_idx(0, 2);
  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());

  TEST_SINGLE_END(1, 1);
}

#define TEST_SINGLE_FAILURE_TIMEOUT(test_interval)                      \
  rak::priority_queue_perform(&torrent::taskScheduler, torrent::cachedTime); \
  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());                       \
  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == test_interval); \
  torrent::cachedTime += rak::timer::from_seconds(test_interval);

void
tracker_controller_test::test_single_failure() {
  torrent::cachedTime = rak::timer::from_seconds(1 << 20);
  TEST_SINGLE_BEGIN();

  TEST_SINGLE_FAILURE_TIMEOUT(5);
  TEST_SINGLE_FAILURE_TIMEOUT(10);
  TEST_SINGLE_FAILURE_TIMEOUT(20);
  TEST_SINGLE_FAILURE_TIMEOUT(40);
  TEST_SINGLE_FAILURE_TIMEOUT(80);
  TEST_SINGLE_FAILURE_TIMEOUT(160);
  TEST_SINGLE_FAILURE_TIMEOUT(320);
  TEST_SINGLE_FAILURE_TIMEOUT(320);

  // TODO: Test with cachedTime not rounded to second.

  // Test also with a low timer value...
  // torrent::cachedTime = rak::timer::from_seconds(1000);

  TEST_SINGLE_END(0, 4);
}

void
tracker_controller_test::test_single_disable() {
  TEST_SINGLE_BEGIN();
  tracker_list.send_state_idx(0, 0);
  TEST_SINGLE_END(0, 0);
}

void
tracker_controller_test::test_send_start() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(start);

  CPPUNIT_ASSERT(!tracker_controller.task_timeout()->is_queued());

  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  CPPUNIT_ASSERT(tracker_0_0->requesting_state() == torrent::Tracker::EVENT_STARTED);

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::mask_send));

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() != 0);

  tracker_controller.send_start_event();
  tracker_controller.disable();

  CPPUNIT_ASSERT(!tracker_0_0->is_busy());

  TEST_SEND_SINGLE_END(1, 0);
}

void
tracker_controller_test::test_send_stop_normal() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  CPPUNIT_ASSERT(tracker_controller.task_timeout()->is_queued());
  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  tracker_controller.send_stop_event();
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == torrent::TrackerController::flag_send_stop);

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == 0);

  tracker_controller.send_stop_event();
  tracker_controller.disable();

  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  tracker_0_0->trigger_success();

  TEST_SEND_SINGLE_END(3, 0);
}

// send_stop during request and right after start, send stop failed.

void
tracker_controller_test::test_send_completed_normal() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  CPPUNIT_ASSERT(tracker_controller.task_timeout()->is_queued());
  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  tracker_controller.send_completed_event();
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == torrent::TrackerController::flag_send_completed);

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == 0);

  tracker_controller.send_completed_event();
  tracker_controller.disable();

  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  tracker_0_0->trigger_success();

  TEST_SEND_SINGLE_END(3, 0);
}

void
tracker_controller_test::test_send_update_normal() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == torrent::TrackerController::flag_send_update);

  CPPUNIT_ASSERT(tracker_controller.task_timeout()->is_queued());
  CPPUNIT_ASSERT(tracker_0_0->latest_event() == torrent::Tracker::EVENT_NONE);

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  TEST_SEND_SINGLE_END(1, 0);
}

void
tracker_controller_test::test_send_task_timeout() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  CPPUNIT_ASSERT(tracker_controller.task_timeout()->is_queued());

  TEST_SEND_SINGLE_END(0, 0);
}

void
tracker_controller_test::test_send_close_on_enable() {
  TRACKER_SETUP();
  TRACKER_INSERT(0, tracker_0);
  TRACKER_INSERT(0, tracker_1);
  TRACKER_INSERT(0, tracker_2);
  TRACKER_INSERT(0, tracker_3);

  tracker_list.send_state_idx(0, torrent::Tracker::EVENT_NONE);
  tracker_list.send_state_idx(1, torrent::Tracker::EVENT_STARTED);
  tracker_list.send_state_idx(2, torrent::Tracker::EVENT_STOPPED);
  tracker_list.send_state_idx(3, torrent::Tracker::EVENT_COMPLETED);

  tracker_controller.enable();

  CPPUNIT_ASSERT(!tracker_0->is_busy());
  CPPUNIT_ASSERT(!tracker_1->is_busy());
  CPPUNIT_ASSERT(!tracker_2->is_busy());
  CPPUNIT_ASSERT(tracker_3->is_busy());
}

void
tracker_controller_test::test_multiple_success() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  TEST_GOTO_NEXT_TIMEOUT(tracker_0_0->normal_interval());
  TEST_MULTI3_IS_BUSY("10000", "10000");

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());
  
  TEST_GOTO_NEXT_TIMEOUT(tracker_0_0->normal_interval());
  TEST_MULTI3_IS_BUSY("10000", "10000");

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  CPPUNIT_ASSERT(!tracker_list.has_active());
  CPPUNIT_ASSERT(tracker_0_0->success_counter() == 3);

  TEST_MULTIPLE_END(3, 0);
}

void
tracker_controller_test::test_multiple_failure() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());
  CPPUNIT_ASSERT(!tracker_controller.is_failure_mode());
  
  TEST_MULTI3_IS_BUSY("01000", "01000");
  CPPUNIT_ASSERT(tracker_0_1->trigger_failure());
  TEST_MULTI3_IS_BUSY("00100", "00100");
  CPPUNIT_ASSERT(tracker_1_0->trigger_success());

  TEST_GOTO_NEXT_TIMEOUT(tracker_0_0->normal_interval());
  TEST_MULTI3_IS_BUSY("10000", "10000");
  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());
  TEST_MULTI3_IS_BUSY("01000", "01000");
  CPPUNIT_ASSERT(tracker_0_1->trigger_failure());

  CPPUNIT_ASSERT(!tracker_controller.is_failure_mode());
  TEST_MULTI3_IS_BUSY("00100", "00100");
  CPPUNIT_ASSERT(tracker_1_0->trigger_failure());
  CPPUNIT_ASSERT(tracker_controller.is_failure_mode());

  TEST_MULTI3_IS_BUSY("00010", "00010");
  CPPUNIT_ASSERT(tracker_2_0->trigger_failure());
  TEST_MULTI3_IS_BUSY("00001", "00001");
  CPPUNIT_ASSERT(tracker_3_0->trigger_failure());

  // Properly tests that we're going into timeouts... Disable remaining trackers.

  // for (int i = 0; i < 5; i++)
  //   std::cout << std::endl << i << ": "
  //             << tracker_list[i]->activity_time_last() << ' ' 
  //             << tracker_list[i]->success_time_last() << ' '
  //             << tracker_list[i]->failed_time_last() << std::endl;

  // Try inserting some delays in order to test the timers.

  TEST_GOTO_NEXT_TIMEOUT(5);
  TEST_MULTI3_IS_BUSY("00100", "00100");
  CPPUNIT_ASSERT(tracker_1_0->trigger_success());
  CPPUNIT_ASSERT(!tracker_controller.is_failure_mode());

  // TEST_GOTO_NEXT_TIMEOUT(tracker_list[0]->normal_interval());
  // TEST_MULTI3_IS_BUSY("01000", "10000");
  // CPPUNIT_ASSERT(tracker_0_1->trigger_success());

  CPPUNIT_ASSERT(tracker_list.count_active() == 0);
  TEST_MULTIPLE_END(2, 7);
}

void
tracker_controller_test::test_multiple_cycle() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());
  CPPUNIT_ASSERT(tracker_0_1->trigger_success());
  CPPUNIT_ASSERT(tracker_list.front() == tracker_0_1);

  TEST_GOTO_NEXT_TIMEOUT(tracker_0_1->normal_interval());
  
  TEST_MULTI3_IS_BUSY("01000", "10000");
  CPPUNIT_ASSERT(tracker_0_1->trigger_success());

  CPPUNIT_ASSERT(tracker_list.count_active() == 0);
  TEST_MULTIPLE_END(2, 1);
}

void
tracker_controller_test::test_multiple_cycle_second_group() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());
  CPPUNIT_ASSERT(tracker_0_1->trigger_failure());
  CPPUNIT_ASSERT(tracker_1_0->trigger_success());

  CPPUNIT_ASSERT(tracker_list[0] == tracker_0_0);
  CPPUNIT_ASSERT(tracker_list[1] == tracker_0_1);
  CPPUNIT_ASSERT(tracker_list[2] == tracker_1_0);
  CPPUNIT_ASSERT(tracker_list[3] == tracker_2_0);
  CPPUNIT_ASSERT(tracker_list[4] == tracker_3_0);

  TEST_MULTIPLE_END(1, 2);
}

void
tracker_controller_test::test_multiple_send_stop() {
  TEST_MULTI3_BEGIN();

  tracker_list.send_state_idx(1, torrent::Tracker::EVENT_NONE);
  tracker_list.send_state_idx(3, torrent::Tracker::EVENT_NONE);
  tracker_list.send_state_idx(4, torrent::Tracker::EVENT_NONE);

  CPPUNIT_ASSERT(tracker_0_1->trigger_success());
  CPPUNIT_ASSERT(tracker_2_0->trigger_success());
  CPPUNIT_ASSERT(tracker_3_0->trigger_success());
  CPPUNIT_ASSERT(tracker_list.count_active() == 0);

  tracker_controller.send_stop_event();
  CPPUNIT_ASSERT(tracker_list.count_active() == 3);

  TEST_MULTI3_IS_BUSY("01011", "10011");
  CPPUNIT_ASSERT(tracker_0_1->trigger_success());
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) != torrent::TrackerController::flag_send_stop);
  CPPUNIT_ASSERT(tracker_2_0->trigger_success());
  CPPUNIT_ASSERT(tracker_3_0->trigger_success());

  CPPUNIT_ASSERT(tracker_list.count_active() == 0);

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
tracker_controller_test::test_multiple_send_update() {
  TEST_MULTI3_BEGIN();

  tracker_controller.send_update_event();
  TEST_GOTO_NEXT_TIMEOUT(0);
  TEST_MULTI3_IS_BUSY("10000", "10000");
 
  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  tracker_0_0->set_failed(1, torrent::cachedTime.seconds());

  tracker_controller.send_update_event();
  TEST_GOTO_NEXT_TIMEOUT(0);
  TEST_MULTI3_IS_BUSY("01000", "01000");

  CPPUNIT_ASSERT(tracker_0_1->trigger_failure());

  TEST_GOTO_NEXT_TIMEOUT(0);
  TEST_MULTI3_IS_BUSY("00100", "00100");

  TEST_MULTIPLE_END(2, 0);
}

void
tracker_controller_test::test_requesting_basic() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  CPPUNIT_ASSERT(tracker_0_0->trigger_success(10, 20));

  tracker_controller.start_requesting();
  TEST_GOTO_NEXT_TIMEOUT(0);
  TEST_MULTI3_IS_BUSY("01111", "01111");

  CPPUNIT_ASSERT(tracker_0_1->trigger_success());
  CPPUNIT_ASSERT(tracker_1_0->trigger_success());
  CPPUNIT_ASSERT(tracker_2_0->trigger_success());
  CPPUNIT_ASSERT(tracker_3_0->trigger_success());

  // TODO: Change this so that requesting state results in tracker
  // requests from many peers. Also, add a limit so we don't keep
  // requesting from spent trackers.

  // Next timeout should be soon...
  TEST_GOTO_NEXT_TIMEOUT(30);
  TEST_MULTI3_IS_BUSY("11111", "11111");

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  tracker_controller.stop_requesting();

  TEST_MULTIPLE_END(6, 0);
}

void
tracker_controller_test::test_requesting_timeout() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  tracker_controller.start_requesting();
  TEST_GOTO_NEXT_TIMEOUT(0);

  TEST_MULTI3_IS_BUSY("11111", "11111");

  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());
  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 5);
  CPPUNIT_ASSERT(tracker_0_1->trigger_failure());
  CPPUNIT_ASSERT(tracker_1_0->trigger_failure());
  CPPUNIT_ASSERT(tracker_2_0->trigger_failure());
  CPPUNIT_ASSERT(tracker_3_0->trigger_failure());

  TEST_GOTO_NEXT_TIMEOUT(5);

  TEST_MULTI3_IS_BUSY("11111", "11111");

  CPPUNIT_ASSERT(!tracker_controller.task_timeout()->is_queued());
  CPPUNIT_ASSERT(tracker_0_1->trigger_success());
  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 30);

  TEST_MULTIPLE_END(1, 5);
}

void
tracker_controller_test::test_promiscious_timeout() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(start);

  TEST_GOTO_NEXT_TIMEOUT(3);

  TEST_MULTI3_IS_BUSY("11111", "11111");

  CPPUNIT_ASSERT(!tracker_controller.task_timeout()->is_queued());

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::flag_promiscuous_mode));

  CPPUNIT_ASSERT(tracker_0_1->trigger_success());
  CPPUNIT_ASSERT(tracker_1_0->trigger_success());
  CPPUNIT_ASSERT(tracker_2_0->trigger_success());
  CPPUNIT_ASSERT(!tracker_controller.task_timeout()->is_queued());

  CPPUNIT_ASSERT(tracker_3_0->trigger_success());
  TEST_GOTO_NEXT_TIMEOUT(tracker_0_0->normal_interval());

  TEST_MULTIPLE_END(5, 0);
}

// Fix failure event handler so that it properly handles multi-request
// situations. This includes fixing old tests.

void
tracker_controller_test::test_promiscious_failed() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(start);

  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::flag_promiscuous_mode));

  TEST_MULTI3_IS_BUSY("01111", "01111");
  CPPUNIT_ASSERT(tracker_controller.task_timeout()->is_queued());

  CPPUNIT_ASSERT(tracker_3_0->trigger_failure());
  torrent::cachedTime += rak::timer::from_seconds(2);
  CPPUNIT_ASSERT(tracker_2_0->trigger_failure());

  TEST_MULTI3_IS_BUSY("01100", "01100");
  TEST_GOTO_NEXT_TIMEOUT(3);
  TEST_MULTI3_IS_BUSY("11101", "11101");

  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());
  CPPUNIT_ASSERT(tracker_0_1->trigger_failure());
  CPPUNIT_ASSERT(tracker_1_0->trigger_failure());
  CPPUNIT_ASSERT(tracker_3_0->trigger_failure());

  CPPUNIT_ASSERT(!tracker_list.has_active());
  CPPUNIT_ASSERT(tracker_controller.task_timeout()->is_queued());

  TEST_MULTIPLE_END(0, 7);
}

// Test timeout called, no usable trackers at all which leads to
// disabling the timeout.
//
// The enable/disable tracker functions will poke the tracker
// controller, ensuring the tast timeout gets re-started.
void
tracker_controller_test::test_timeout_lacking_usable() {
  TEST_MULTI3_BEGIN();

  std::for_each(tracker_list.begin(), tracker_list.end(), std::mem_fun(&torrent::Tracker::disable));
  CPPUNIT_ASSERT(tracker_controller.task_timeout()->is_queued());

  TEST_GOTO_NEXT_TIMEOUT(0);

  TEST_MULTI3_IS_BUSY("00000", "00000");
  CPPUNIT_ASSERT(!tracker_controller.task_timeout()->is_queued());

  tracker_1_0->enable();

  CPPUNIT_ASSERT(tracker_controller.task_timeout()->is_queued());
  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);

  TEST_GOTO_NEXT_TIMEOUT(0);

  TEST_MULTI3_IS_BUSY("00100", "00100");

  CPPUNIT_ASSERT(!tracker_controller.task_timeout()->is_queued());
  tracker_3_0->enable();
  CPPUNIT_ASSERT(!tracker_controller.task_timeout()->is_queued());

  CPPUNIT_ASSERT(enabled_counter == 5 + 2 && disabled_counter == 5);
  TEST_MULTIPLE_END(0, 0);
}

void
tracker_controller_test::test_disable_tracker() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(update);

  CPPUNIT_ASSERT(tracker_controller.task_timeout()->is_queued());
  CPPUNIT_ASSERT(tracker_0_0->is_busy());

  tracker_0_0->disable();

  CPPUNIT_ASSERT(!tracker_0_0->is_busy());
  CPPUNIT_ASSERT(tracker_controller.task_timeout()->is_queued());

  TEST_SINGLE_END(0, 0);
}

void
tracker_controller_test::test_new_peers() {
  TRACKER_SETUP();
  TRACKER_INSERT(0, tracker_0);

  tracker_list.send_state_idx(0, torrent::Tracker::EVENT_NONE);

  CPPUNIT_ASSERT(tracker_0->trigger_success(10));
  CPPUNIT_ASSERT(tracker_0->latest_new_peers() == 10);

  tracker_controller.enable();

  TEST_GOTO_NEXT_TIMEOUT(0);
  CPPUNIT_ASSERT(tracker_0->trigger_success(20));
  CPPUNIT_ASSERT(tracker_0->latest_new_peers() == 20);
}

void
tracker_controller_test::test_scrape_basic() {
  TEST_MULTI3_BEGIN();
  tracker_controller.disable();

  CPPUNIT_ASSERT(!tracker_controller.task_scrape()->is_queued());
  tracker_0_1->set_can_scrape();
  tracker_1_0->set_can_scrape();
  
  tracker_controller.scrape_request(0);

  TEST_MULTI3_IS_BUSY("00000", "00000");
  CPPUNIT_ASSERT(!tracker_controller.task_timeout()->is_queued());
  CPPUNIT_ASSERT(tracker_controller.task_scrape()->is_queued());
  CPPUNIT_ASSERT(tracker_0_1->latest_event() == torrent::Tracker::EVENT_NONE);
  CPPUNIT_ASSERT(tracker_1_0->latest_event() == torrent::Tracker::EVENT_NONE);

  TEST_GOTO_NEXT_SCRAPE(0);

  TEST_MULTI3_IS_BUSY("01100", "01100");
  CPPUNIT_ASSERT(!tracker_controller.task_timeout()->is_queued());
  CPPUNIT_ASSERT(!tracker_controller.task_scrape()->is_queued());
  CPPUNIT_ASSERT(tracker_0_1->latest_event() == torrent::Tracker::EVENT_SCRAPE);
  CPPUNIT_ASSERT(tracker_1_0->latest_event() == torrent::Tracker::EVENT_SCRAPE);

  CPPUNIT_ASSERT(tracker_0_1->trigger_scrape());
  CPPUNIT_ASSERT(tracker_1_0->trigger_scrape());

  TEST_MULTI3_IS_BUSY("00000", "00000");
  CPPUNIT_ASSERT(!tracker_controller.task_timeout()->is_queued());
  CPPUNIT_ASSERT(!tracker_controller.task_scrape()->is_queued());

  CPPUNIT_ASSERT(tracker_0_1->scrape_time_last() != 0);
  CPPUNIT_ASSERT(tracker_1_0->scrape_time_last() != 0);

  TEST_SINGLE_END(0, 0);
}

void
tracker_controller_test::test_scrape_priority() {
  TEST_SINGLE_BEGIN();
  TEST_GOTO_NEXT_TIMEOUT(0);
  tracker_0_0->trigger_success();
  tracker_0_0->set_can_scrape();

  tracker_controller.scrape_request(0);

  TEST_GOTO_NEXT_SCRAPE(0);
  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  CPPUNIT_ASSERT(tracker_0_0->latest_event() == torrent::Tracker::EVENT_SCRAPE);

  // Check the other event types too?
  tracker_controller.send_update_event();

  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  CPPUNIT_ASSERT(tracker_0_0->latest_event() == torrent::Tracker::EVENT_NONE);

  CPPUNIT_ASSERT(tracker_controller.task_timeout()->is_queued());
  CPPUNIT_ASSERT(!tracker_controller.task_scrape()->is_queued());

  tracker_0_0->trigger_success();

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() > 1);

  torrent::cachedTime += rak::timer::from_seconds(tracker_controller.seconds_to_next_timeout() - 1);
  rak::priority_queue_perform(&torrent::taskScheduler, torrent::cachedTime);

  tracker_controller.scrape_request(0);
  TEST_GOTO_NEXT_SCRAPE(0);

  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  CPPUNIT_ASSERT(tracker_0_0->latest_event() == torrent::Tracker::EVENT_SCRAPE);

  TEST_GOTO_NEXT_TIMEOUT(1);

  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  CPPUNIT_ASSERT(tracker_0_0->latest_event() == torrent::Tracker::EVENT_NONE);

  TEST_SINGLE_END(2, 0);
}

// We should not request scrape from more than one tracker per group.


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

