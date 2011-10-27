#include "config.h"

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
                                                                        \
  tracker_controller.slot_success() = std::bind(&increment_value, &success_counter); \
  tracker_controller.slot_failure() = std::bind(&increment_value, &failure_counter); \
  tracker_controller.slot_timeout() = std::bind(&increment_value, &timeout_counter); \
                                                                        \
  tracker_list.slot_success() = std::bind(&torrent::TrackerController::receive_success_new, &tracker_controller, std::placeholders::_1, std::placeholders::_2); \
  tracker_list.slot_failure() = std::bind(&torrent::TrackerController::receive_failure_new, &tracker_controller, std::placeholders::_1, std::placeholders::_2);

#define TRACKER_INSERT(group, name)                             \
  TrackerTest* name = new TrackerTest(&tracker_list, "");       \
  tracker_list.insert(group, name);

#define TEST_SINGLE_BEGIN()                                             \
  TRACKER_SETUP();                                                      \
  TRACKER_INSERT(0, tracker_0_0);                                       \
                                                                        \
  tracker_controller.enable();                                          \
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::mask_send)); \

#define TEST_SINGLE_END(succeeded, failed)                              \
  tracker_controller.disable();                                         \
  CPPUNIT_ASSERT(!tracker_list.has_active());                           \
  CPPUNIT_ASSERT(success_counter == succeeded &&                        \
                 failure_counter == failure_counter);

#define TEST_SEND_SINGLE_BEGIN()                                        \
  tracker_controller.send_start_event();                                \
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == \
                 torrent::TrackerController::flag_send_start);          \
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
                 failure_counter == failure_counter);

#define TEST_GOTO_NEXT_TIMEOUT(assumed_timeout)                         \
  CPPUNIT_ASSERT(assumed_timeout == tracker_controller.seconds_to_next_timeout()); \
  torrent::cachedTime += rak::timer::from_seconds(tracker_controller.seconds_to_next_timeout()); \
  rak::priority_queue_perform(&torrent::taskScheduler, torrent::cachedTime);


static void increment_value(int* value) { (*value)++; }

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
  CPPUNIT_ASSERT(tracker_controller.failed_requests() == 0);
  CPPUNIT_ASSERT(!tracker_controller.is_active());
  CPPUNIT_ASSERT(!tracker_controller.is_requesting());
}

void
tracker_controller_test::test_enable() {
  torrent::TrackerController tracker_controller(NULL);

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
  TEST_SINGLE_BEGIN();

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
  CPPUNIT_ASSERT(tracker_controller.failed_requests() == 0);
  
  tracker_list.send_state_idx(0, 2);
  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());

  //  CPPUNIT_ASSERT(tracker_controller.failed_requests() == 1);
  TEST_SINGLE_END(1, 1);
}

#define TEST_SINGLE_FAILURE_TIMEOUT(test_interval)                      \
  rak::priority_queue_perform(&torrent::taskScheduler, torrent::cachedTime); \
  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());                       \
  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == test_interval); \
  torrent::cachedTime += rak::timer::from_seconds(test_interval);

void
tracker_controller_test::test_single_failure() {
  torrent::cachedTime = rak::timer::from_seconds(1000);;
  TEST_SINGLE_BEGIN();

  TEST_SINGLE_FAILURE_TIMEOUT(5);
  TEST_SINGLE_FAILURE_TIMEOUT(10);
  TEST_SINGLE_FAILURE_TIMEOUT(20);
  TEST_SINGLE_FAILURE_TIMEOUT(40);
  TEST_SINGLE_FAILURE_TIMEOUT(80);
  TEST_SINGLE_FAILURE_TIMEOUT(160);
  TEST_SINGLE_FAILURE_TIMEOUT(320);
  TEST_SINGLE_FAILURE_TIMEOUT(320);

  TEST_SINGLE_END(0, 4);
}

void
tracker_controller_test::test_single_disable() {
  TEST_SINGLE_BEGIN();
  tracker_list.send_state_idx(0, 1);
  TEST_SINGLE_END(0, 0);
}

void
tracker_controller_test::test_send_start() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN();

  // We might want some different types of timeouts... Move these test to a promicious unit test...
  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);
  //CPPUNIT_ASSERT(tracker_controller.seconds_to_promicious_mode() != 0);

  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  CPPUNIT_ASSERT(tracker_0_0->requesting_state() == torrent::Tracker::EVENT_STARTED);

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::mask_send));

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() != 0);
  //CPPUNIT_ASSERT(tracker_controller.seconds_to_promicious_mode() != 0);

  TEST_SEND_SINGLE_END(1, 0);
}

void
tracker_controller_test::test_send_stop_normal() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN();

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  tracker_controller.send_stop_event();
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == torrent::TrackerController::flag_send_stop);

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);
  // CPPUNIT_ASSERT(tracker_controller.seconds_to_promicious_mode() == 0);

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == 0);

  // Add a slot for stopped events done.
  TEST_SEND_SINGLE_END(2, 0);
}

// send_stop during request and right after start, send stop failed.

void
tracker_controller_test::test_send_completed_normal() {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN();

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  tracker_controller.send_completed_event();
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == torrent::TrackerController::flag_send_completed);

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);
  // CPPUNIT_ASSERT(tracker_controller.seconds_to_promicious_mode() == 0);

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == 0);

  // Add a slot for stopped events done.
  TEST_SEND_SINGLE_END(2, 0);
}

void
tracker_controller_test::test_multiple_success() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN();

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());
  TEST_GOTO_NEXT_TIMEOUT(tracker_0_0->normal_interval());

  // Verify that we're busy...
  CPPUNIT_ASSERT(tracker_0_0->is_busy());

  CPPUNIT_ASSERT(std::find_if(tracker_list.begin() + 1, tracker_list.end(),
                              std::mem_fun(&torrent::Tracker::is_busy)) == tracker_list.end());

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());
  
  CPPUNIT_ASSERT(!tracker_list.has_active());
  CPPUNIT_ASSERT(tracker_0_0->success_counter() == 2);

  TEST_MULTIPLE_END(2, 0);
}

void
tracker_controller_test::test_multiple_failure() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN();

  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());
  
  TEST_GOTO_NEXT_TIMEOUT(tracker_controller.seconds_to_next_timeout());

  // Verify that the retry timer is _fast_ when the next tracker has
  // no failed counter.

  CPPUNIT_ASSERT(tracker_list.count_active() == 1);
  CPPUNIT_ASSERT(tracker_0_1->is_busy());
  CPPUNIT_ASSERT(tracker_0_1->trigger_failure());

  TEST_GOTO_NEXT_TIMEOUT(tracker_controller.seconds_to_next_timeout());
  
  CPPUNIT_ASSERT(tracker_list.count_active() == 1 && tracker_1_0->is_busy());
  CPPUNIT_ASSERT(tracker_1_0->trigger_success());

  // Also verify the next time out, etc...

  CPPUNIT_ASSERT(tracker_list.count_active() == 0);
  TEST_MULTIPLE_END(1, 2);
}

void
tracker_controller_test::test_multiple_cycle() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN();

  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());
  TEST_GOTO_NEXT_TIMEOUT(tracker_controller.seconds_to_next_timeout());

  CPPUNIT_ASSERT(tracker_0_1->trigger_success());
  CPPUNIT_ASSERT(tracker_list.front() == tracker_0_1);

  TEST_GOTO_NEXT_TIMEOUT(tracker_controller.seconds_to_next_timeout());
  
  CPPUNIT_ASSERT(tracker_list.count_active() == 1 && tracker_0_1->is_busy());
  CPPUNIT_ASSERT(tracker_0_1->trigger_success());

  CPPUNIT_ASSERT(tracker_list.count_active() == 0);
  TEST_MULTIPLE_END(2, 1);
}

void
tracker_controller_test::test_multiple_cycle_second_group() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN();

  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());
  TEST_GOTO_NEXT_TIMEOUT(tracker_controller.seconds_to_next_timeout());
  CPPUNIT_ASSERT(tracker_0_1->trigger_failure());
  TEST_GOTO_NEXT_TIMEOUT(tracker_controller.seconds_to_next_timeout());

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

  CPPUNIT_ASSERT(!tracker_0_0->is_busy());
  CPPUNIT_ASSERT(!tracker_1_0->is_busy());
  CPPUNIT_ASSERT(tracker_0_1->trigger_success());
  CPPUNIT_ASSERT(tracker_2_0->trigger_success());
  CPPUNIT_ASSERT(tracker_3_0->trigger_success());

  CPPUNIT_ASSERT(tracker_list.count_active() == 0);

  // Test also that after closing the tracker controller, and
  // reopening it a 'send stop' event causes no tracker to be busy.

  TEST_MULTIPLE_END(6, 0);
}

void
tracker_controller_test::test_multiple_requesting() {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN();

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  tracker_controller.start_requesting();

  // Next timeout should be short...
  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);
  TEST_GOTO_NEXT_TIMEOUT(tracker_controller.seconds_to_next_timeout());

  // Should be connecting again to... same tracker.
  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  
  // When adding multi-tracker requests; check that all trackers are
  // requesting.
  CPPUNIT_ASSERT(!tracker_0_1->is_busy());
  CPPUNIT_ASSERT(!tracker_1_0->is_busy());
  CPPUNIT_ASSERT(!tracker_2_0->is_busy());
  CPPUNIT_ASSERT(!tracker_3_0->is_busy());

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  // Consider adding

  // Next timeout should be soon...
  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 30);
  TEST_GOTO_NEXT_TIMEOUT(tracker_controller.seconds_to_next_timeout());

  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  CPPUNIT_ASSERT(!tracker_0_1->is_busy());
  CPPUNIT_ASSERT(!tracker_1_0->is_busy());
  CPPUNIT_ASSERT(!tracker_2_0->is_busy());
  CPPUNIT_ASSERT(!tracker_3_0->is_busy());

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  tracker_controller.stop_requesting();

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == tracker_0_0->normal_interval());
  TEST_MULTIPLE_END(3, 0);
}

// Add multiple trackers, with partial-promiscious mode that is actually
// sequential. Test multiple stopped/completed to trackers based on who
// knows we're running.



// Test send_* with controller not enabled.

// Test cleanup after disable.
// Test cleanup of timers at disable (and empty timers at enable).

// Test trying to send_start twice, etc.

// Test send_start promiscious...
//   - Make sure we check that no timer is inserted while still having active trackers.
//   - Calculate the next timeout according to a list of in-use trackers, with the first timeout as the interval.

// Test clearing of recv/failed counter on trackers.
