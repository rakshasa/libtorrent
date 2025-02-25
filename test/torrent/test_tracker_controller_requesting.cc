#include "config.h"

#include <functional>
#include <iostream>

#include "rak/priority_queue_default.h"
#include "test/torrent/test_tracker_controller_requesting.h"
#include "test/torrent/test_tracker_list.h"

#include "globals.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_tracker_controller_requesting);

void
test_tracker_controller_requesting::setUp() {
  test_fixture::setUp();

  CPPUNIT_ASSERT(torrent::taskScheduler.empty());

  torrent::cachedTime = rak::timer::current();
}

void
test_tracker_controller_requesting::tearDown() {
  torrent::taskScheduler.clear();

  test_fixture::tearDown();
}

void
test_tracker_controller_requesting::do_test_hammering_basic(bool success1, bool success2, bool success3, uint32_t min_interval) {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(start);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);

  if (min_interval != 0)
    tracker_0_0_worker->set_new_min_interval(min_interval);
  else
    tracker_0_0_worker->set_new_min_interval(600);

  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  CPPUNIT_ASSERT(success1 ? tracker_0_0_worker->trigger_success() : tracker_0_0_worker->trigger_failure());

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == tracker_0_0->state().normal_interval());
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::flag_promiscuous_mode));

  tracker_controller.start_requesting();

  CPPUNIT_ASSERT(!tracker_0_0->is_busy());
  CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, 0));
  CPPUNIT_ASSERT(!tracker_0_0->is_busy());

  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::flag_requesting));
  CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, tracker_0_0->state().min_interval()));

  CPPUNIT_ASSERT(tracker_0_0->is_busy());

  if (success2) {
    CPPUNIT_ASSERT(tracker_0_0_worker->trigger_success());

    CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, 30));
    CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, tracker_0_0->state().min_interval() - 30));
  } else {
    CPPUNIT_ASSERT(tracker_0_0_worker->trigger_failure());

    CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, 5));
    CPPUNIT_ASSERT(tracker_0_0->is_busy());
  }

  tracker_controller.stop_requesting();

  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  CPPUNIT_ASSERT(success3 ? tracker_0_0_worker->trigger_success() : tracker_0_0_worker->trigger_failure());

  TEST_SINGLE_END(success1 + success2 + success3, !success1 + !success2 + !success3);
}

void
test_tracker_controller_requesting::test_hammering_basic_success() {
  do_test_hammering_basic(true, true, true);
}

void
test_tracker_controller_requesting::test_hammering_basic_success_long_timeout() {
  do_test_hammering_basic(true, true, true, 1000);
}

void
test_tracker_controller_requesting::test_hammering_basic_success_short_timeout() {
  do_test_hammering_basic(true, true, true, 300);
}

void
test_tracker_controller_requesting::test_hammering_basic_failure() {
  do_test_hammering_basic(true, false, false);
}

void
test_tracker_controller_requesting::test_hammering_basic_failure_long_timeout() {
  do_test_hammering_basic(true, false, false, 1000);
}

void
test_tracker_controller_requesting::test_hammering_basic_failure_short_timeout() {
  do_test_hammering_basic(true, false, false, 300);
}

// Differentiate between failure connection / http error and tracker returned error.

void
test_tracker_controller_requesting::do_test_hammering_multi3(bool success1, bool success2, bool success3, uint32_t min_interval) {
  TEST_MULTI3_BEGIN();
  TEST_SEND_SINGLE_BEGIN(start);

  auto tracker_0_0_worker = TrackerTest::test_worker(tracker_0_0);
  auto tracker_2_0_worker = TrackerTest::test_worker(tracker_2_0);

  if (min_interval != 0)
    tracker_0_0_worker->set_new_min_interval(min_interval);
  else
    tracker_0_0_worker->set_new_min_interval(600);

  TEST_MULTI3_IS_BUSY("10000", "10000");
  CPPUNIT_ASSERT(success1 ? tracker_0_0_worker->trigger_success() : tracker_0_0_worker->trigger_failure());

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == tracker_0_0->state().normal_interval());
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::flag_promiscuous_mode));

  tracker_controller.start_requesting();

  TEST_MULTI3_IS_BUSY("00000", "00000");
  CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, 0));
  TEST_MULTI3_IS_BUSY("00111", "00111");

  if (success2)
    CPPUNIT_ASSERT(tracker_2_0_worker->trigger_success());
  else
    CPPUNIT_ASSERT(tracker_2_0_worker->trigger_failure());

  CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, 30));
  TEST_MULTI3_IS_BUSY("00101", "00101");

  TrackerTest* next_tracker = tracker_0_0_worker;
  unsigned int next_timeout = next_tracker->test_state().min_interval();
  const char*  next_is_busy = "10111";

  if (tracker_0_0->state().min_interval() < tracker_2_0->state().min_interval()) {
    CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, next_tracker->test_state().min_interval() - 30));
    TEST_MULTI3_IS_BUSY("10101", "10101");
  } else if (tracker_0_0->state().min_interval() > tracker_2_0->state().min_interval()) {
    next_tracker = tracker_2_0_worker;
    next_timeout = tracker_0_0->state().min_interval() - tracker_2_0->state().min_interval();
    next_is_busy = "10101";
    CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, next_tracker->test_state().min_interval() - 30));
    TEST_MULTI3_IS_BUSY("00111", "00111");
  } else {
    CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, next_tracker->test_state().min_interval() - 30));
    TEST_MULTI3_IS_BUSY("10111", "10111");
  }

  if (success2) {
    CPPUNIT_ASSERT(next_tracker->trigger_success());

    CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, 30));
    CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, next_timeout - 30));

    TEST_MULTI3_IS_BUSY(next_is_busy, next_is_busy);
  } else {
    CPPUNIT_ASSERT(next_tracker->trigger_failure());

    CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, 5));
    TEST_MULTI3_IS_BUSY("10000", "10000");
  }

  tracker_controller.stop_requesting();

  TEST_MULTI3_IS_BUSY(next_is_busy, next_is_busy);
  CPPUNIT_ASSERT(success3 ? tracker_0_0_worker->trigger_success() : tracker_0_0_worker->trigger_failure());

  TEST_MULTIPLE_END(success1 + 2*success2 + success3, !success1 + 2*!success2 + !success3);
}

void
test_tracker_controller_requesting::test_hammering_multi_success() {
  do_test_hammering_multi3(true, true, true);
}

void
test_tracker_controller_requesting::test_hammering_multi_success_long_timeout() {
  do_test_hammering_multi3(true, true, true, 1000);
}

void
test_tracker_controller_requesting::test_hammering_multi_success_short_timeout() {
  do_test_hammering_multi3(true, true, true, 300);
}

void
test_tracker_controller_requesting::test_hammering_multi_failure() {
}
