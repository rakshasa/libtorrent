#include "config.h"

#include <iostream>
#include <tr1/functional>

#include "rak/priority_queue_default.h"

#include "globals.h"
#include "tracker_list_test.h"
#include "tracker_controller_requesting.h"

namespace tr1 { using namespace std::tr1; }

CPPUNIT_TEST_SUITE_REGISTRATION(tracker_controller_requesting);

void
tracker_controller_requesting::setUp() {
  CPPUNIT_ASSERT(torrent::taskScheduler.empty());

  torrent::cachedTime = rak::timer::current();
}

void
tracker_controller_requesting::tearDown() {
  torrent::taskScheduler.clear();
}

void
do_test_hammering_basic(bool success1, bool success2, bool success3, uint32_t min_interval = 0) {
  TEST_SINGLE_BEGIN();
  TEST_SEND_SINGLE_BEGIN(start);

  if (min_interval != 0)
    tracker_0_0->set_new_min_interval(min_interval);

  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  CPPUNIT_ASSERT(success1 ? tracker_0_0->trigger_success() : tracker_0_0->trigger_failure());

  CPPUNIT_ASSERT(tracker_controller.seconds_to_next_timeout() == tracker_0_0->normal_interval());
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::flag_promiscuous_mode));

  tracker_controller.start_requesting();

  CPPUNIT_ASSERT(!tracker_0_0->is_busy());
  CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, 0));
  CPPUNIT_ASSERT(!tracker_0_0->is_busy());

  CPPUNIT_ASSERT((tracker_controller.flags() & torrent::TrackerController::flag_requesting));
  CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, tracker_0_0->min_interval()));
  
  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  CPPUNIT_ASSERT(success2 ? tracker_0_0->trigger_success() : tracker_0_0->trigger_failure());

  CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, 30));
  CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, tracker_0_0->min_interval() - 30));

  tracker_controller.stop_requesting();

  CPPUNIT_ASSERT(tracker_0_0->is_busy());
  CPPUNIT_ASSERT(success3 ? tracker_0_0->trigger_success() : tracker_0_0->trigger_failure());

  TEST_SINGLE_END(success1 + success2 + success3, !success1 + !success2 + !success3);
}

void
tracker_controller_requesting::test_hammering_basic_success() {
  do_test_hammering_basic(true, true, true);
}

void
tracker_controller_requesting::test_hammering_basic_success_new_timeout() {
  do_test_hammering_basic(true, true, true, 1000);
}

void
tracker_controller_requesting::test_hammering_basic_failure() {
  do_test_hammering_basic(true, false, false);
}
void
tracker_controller_requesting::test_hammering_basic_failure_new_timeout() {
  do_test_hammering_basic(true, false, false, 1000);
}

// Differentiate between failure connection / http error and tracker returned error.
