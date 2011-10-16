#include "config.h"

#include <torrent/tracker_controller.h>

#include "rak/priority_queue_default.h"

#include "tracker_list_test.h"
#include "tracker_controller_test.h"

namespace std { using namespace tr1; }

CPPUNIT_TEST_SUITE_REGISTRATION(tracker_controller_test);

void
tracker_controller_test::setUp() {
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
  torrent::TrackerController tracker_controller(NULL);

  tracker_controller.enable();
  tracker_controller.start_requesting();

  CPPUNIT_ASSERT(tracker_controller.is_active());
  CPPUNIT_ASSERT(tracker_controller.is_requesting());

  tracker_controller.stop_requesting();

  CPPUNIT_ASSERT(tracker_controller.is_active());
  CPPUNIT_ASSERT(!tracker_controller.is_requesting());
}

#define TRACKER_SETUP()                                                 \
  torrent::TrackerList tracker_list;                                    \
  torrent::TrackerController tracker_controller(&tracker_list);         \
                                                                        \
  int success_counter = 0;                                              \
  int failure_counter = 0;                                              \
                                                                        \
  tracker_controller.task_timeout()->set_slot(rak::mem_fn(&tracker_controller, &torrent::TrackerController::receive_task_timeout)); \
                                                                        \
  tracker_controller.slot_success() = std::bind(&increment_value, &success_counter); \
  tracker_controller.slot_failure() = std::bind(&increment_value, &failure_counter); \
                                                                        \
  tracker_list.slot_success() = std::bind(&torrent::TrackerController::receive_success_new, &tracker_controller, std::placeholders::_1, std::placeholders::_2); \
  tracker_list.slot_failure() = std::bind(&torrent::TrackerController::receive_failure_new, &tracker_controller, std::placeholders::_1, std::placeholders::_2);


#define TRACKER_INSERT(group, name)                             \
  TrackerTest* name = new TrackerTest(&tracker_list, "");       \
  tracker_list.insert(group, name);

static void increment_value(int* value) { (*value)++; }

void
tracker_controller_test::test_single_success() {
  TRACKER_SETUP();
  TRACKER_INSERT(0, tracker_0_0);

  tracker_controller.enable();
  
  tracker_list.send_state_idx(0, 1);
  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  CPPUNIT_ASSERT(success_counter == 1 && failure_counter == 0);
  CPPUNIT_ASSERT(tracker_controller.failed_requests() == 0);
  
  tracker_list.send_state_idx(0, 2);
  CPPUNIT_ASSERT(tracker_0_0->trigger_failure());

  CPPUNIT_ASSERT(success_counter == 1 && failure_counter == 1);
  //  CPPUNIT_ASSERT(tracker_controller.failed_requests() == 1);

  tracker_controller.disable();
}

#define TEST_SEND_SINGLE_BEGIN()                                        \
  TRACKER_SETUP();                                                      \
  TRACKER_INSERT(0, tracker_0_0);                                       \
                                                                        \
  tracker_controller.enable();                                          \
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::mask_send)); \
                                                                        \
  tracker_controller.send_start_event();                                \
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == torrent::TrackerController::flag_send_start); \
                                                                        \
  CPPUNIT_ASSERT(tracker_controller.is_active());                       \
  CPPUNIT_ASSERT(tracker_controller.tracker_list()->has_active());

#define TEST_SEND_SINGLE_END()                                          \
  tracker_controller.disable();                                         \
                                                                        \
  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);    \
  //CPPUNIT_ASSERT(tracker_controller.seconds_to_promicious_mode() != 0);

void
tracker_controller_test::test_send_start() {
  TEST_SEND_SINGLE_BEGIN();

  // We might want some different types of timeouts... Move these test to a promicious unit test...
  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);
  //CPPUNIT_ASSERT(tracker_controller.seconds_to_promicious_mode() != 0);

  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  CPPUNIT_ASSERT(tracker_0_0->requesting_state() == torrent::Tracker::EVENT_STARTED);

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());
  CPPUNIT_ASSERT(success_counter == 1 && failure_counter == 0);
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::mask_send));

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() != 0);
  //CPPUNIT_ASSERT(tracker_controller.seconds_to_promicious_mode() != 0);

  TEST_SEND_SINGLE_END();
}

void
tracker_controller_test::test_send_stop_normal() {
  TEST_SEND_SINGLE_BEGIN();

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());

  tracker_controller.send_stop_event();
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == torrent::TrackerController::flag_send_stop);

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == 0);
  // CPPUNIT_ASSERT(tracker_controller.seconds_to_promicious_mode() == 0);

  CPPUNIT_ASSERT(tracker_0_0->trigger_success());
  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::mask_send) == 0);

  CPPUNIT_ASSERT(success_counter == 2 && failure_counter == 0);
  
  // Add a slot for stopped events done.
  TEST_SEND_SINGLE_END();
}

// send_stop during request and right after start, send stop failed.

// Test send_start promiscious...
//   - Make sure we check that no timer is inserted while still having active trackers.
//   - Calculate the next timeout according to a list of in-use trackers, with the first timeout as the interval.


// Test send_* with controller not enabled.

// Test cleanup after disable.
// Test cleanup of timers at disable (and empty timers at enable).

// Test trying to send_start twice, etc.

// Test quick connect, few peers, request more...

// Test clearing of recv/failed counter on trackers.
