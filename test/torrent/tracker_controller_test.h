#include <cppunit/extensions/HelperMacros.h>

#include "torrent/tracker_controller.h"

class tracker_controller_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(tracker_controller_test);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_enable);
  CPPUNIT_TEST(test_requesting);
  CPPUNIT_TEST(test_timeout);

  CPPUNIT_TEST(test_single_success);
  CPPUNIT_TEST(test_single_failure);
  CPPUNIT_TEST(test_single_disable);

  CPPUNIT_TEST(test_send_start);
  CPPUNIT_TEST(test_send_stop_normal);
  CPPUNIT_TEST(test_send_completed_normal);
  CPPUNIT_TEST(test_send_update_normal);
  CPPUNIT_TEST(test_send_update_failure);
  CPPUNIT_TEST(test_send_task_timeout);
  CPPUNIT_TEST(test_send_close_on_enable);

  CPPUNIT_TEST(test_multiple_success);
  CPPUNIT_TEST(test_multiple_failure);
  CPPUNIT_TEST(test_multiple_cycle);
  CPPUNIT_TEST(test_multiple_cycle_second_group);
  CPPUNIT_TEST(test_multiple_send_stop);

  CPPUNIT_TEST(test_timeout_lacking_usable);
  CPPUNIT_TEST(test_disable_tracker);
  CPPUNIT_TEST(test_new_peers);

  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
  void test_enable();
  void test_disable();
  void test_requesting();
  void test_timeout();

  void test_single_success();
  void test_single_failure();
  void test_single_disable();

  void test_send_start();
  void test_send_stop_normal();
  void test_send_completed_normal();
  void test_send_update_normal();
  void test_send_update_failure();
  void test_send_task_timeout();
  void test_send_close_on_enable();

  void test_multiple_success();
  void test_multiple_failure();
  void test_multiple_cycle();
  void test_multiple_cycle_second_group();
  void test_multiple_send_stop();
  void test_multiple_send_update();

  void test_timeout_lacking_usable();
  void test_disable_tracker();
  void test_new_peers();
};

#define TRACKER_CONTROLLER_SETUP()                                      \
  torrent::TrackerList tracker_list;                                    \
  torrent::TrackerController tracker_controller(&tracker_list);         \
                                                                        \
  int success_counter = 0;                                              \
  int failure_counter = 0;                                              \
  int timeout_counter = 0;                                              \
  int enabled_counter = 0;                                              \
  int disabled_counter = 0;                                             \
                                                                        \
  tracker_controller.slot_success() = std::bind(&increment_value_uint, &success_counter); \
  tracker_controller.slot_failure() = std::bind(&increment_value_void, &failure_counter); \
  tracker_controller.slot_timeout() = std::bind(&increment_value_void, &timeout_counter); \
  tracker_controller.slot_tracker_enabled() = std::bind(&increment_value_void, &enabled_counter); \
  tracker_controller.slot_tracker_disabled() = std::bind(&increment_value_void, &disabled_counter); \
                                                                        \
  tracker_list.slot_success() = std::bind(&torrent::TrackerController::receive_success, &tracker_controller, std::placeholders::_1, std::placeholders::_2); \
  tracker_list.slot_failure() = std::bind(&torrent::TrackerController::receive_failure, &tracker_controller, std::placeholders::_1, std::placeholders::_2); \
  tracker_list.slot_tracker_enabled()  = std::bind(&torrent::TrackerController::receive_tracker_enabled, &tracker_controller, std::placeholders::_1); \
  tracker_list.slot_tracker_disabled() = std::bind(&torrent::TrackerController::receive_tracker_disabled, &tracker_controller, std::placeholders::_1);

#define TEST_SINGLE_BEGIN()                                             \
  TRACKER_CONTROLLER_SETUP();                                           \
  TRACKER_INSERT(0, tracker_0_0);                                       \
                                                                        \
  tracker_controller.enable();                                          \
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::mask_send)); \

#define TEST_SINGLE_END(succeeded, failed)                              \
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
  TRACKER_CONTROLLER_SETUP();                                           \
  TRACKER_INSERT(0, tracker_0_0);                                       \
  TRACKER_INSERT(0, tracker_0_1);                                       \
  TRACKER_INSERT(1, tracker_1_0);                                       \
  TRACKER_INSERT(2, tracker_2_0);                                       \
  TRACKER_INSERT(3, tracker_3_0);                                       \
                                                                        \
  tracker_controller.enable();                                          \
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::mask_send)); \

#define TEST_GROUP_BEGIN()                                              \
  TRACKER_CONTROLLER_SETUP();                                           \
  TRACKER_INSERT(0, tracker_0_0);                                       \
  TRACKER_INSERT(0, tracker_0_1);                                       \
  TRACKER_INSERT(0, tracker_0_2);                                       \
  TRACKER_INSERT(1, tracker_1_0);                                       \
  TRACKER_INSERT(1, tracker_1_1);                                       \
  TRACKER_INSERT(2, tracker_2_0);                                       \
                                                                        \
  tracker_controller.enable();                                          \
  CPPUNIT_ASSERT(!(tracker_controller.flags() & torrent::TrackerController::mask_send)); \

#define TEST_MULTIPLE_END(succeeded, failed)                            \
  tracker_controller.disable();                                         \
  CPPUNIT_ASSERT(!tracker_list.has_active());                           \
  CPPUNIT_ASSERT(success_counter == succeeded &&                        \
                 failure_counter == failed);

#define TEST_GOTO_NEXT_SCRAPE(assumed_scrape)                           \
  CPPUNIT_ASSERT(tracker_controller.task_scrape()->is_queued());        \
  CPPUNIT_ASSERT(assumed_scrape == tracker_controller.seconds_to_next_scrape()); \
  CPPUNIT_ASSERT(test_goto_next_timeout(&tracker_controller, assumed_scrape, true));

bool test_goto_next_timeout(torrent::TrackerController* tracker_controller, uint32_t assumed_timeout, bool is_scrape = false);
