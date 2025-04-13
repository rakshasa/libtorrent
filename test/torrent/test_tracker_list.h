#include "test/helpers/test_fixture.h"
#include "test/helpers/test_main_thread.h"
#include "test/helpers/tracker_test.h"

class test_tracker_list : public test_fixture {
  CPPUNIT_TEST_SUITE(test_tracker_list);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_enable);
  CPPUNIT_TEST(test_close);

  CPPUNIT_TEST(test_tracker_flags);
  CPPUNIT_TEST(test_find_url);
  CPPUNIT_TEST(test_can_scrape);

  CPPUNIT_TEST(test_single_success);
  CPPUNIT_TEST(test_single_failure);
  CPPUNIT_TEST(test_single_closing);

  CPPUNIT_TEST(test_multiple_success);

  CPPUNIT_TEST(test_scrape_success);
  CPPUNIT_TEST(test_scrape_failure);

  CPPUNIT_TEST(test_has_active);

  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown();

  void test_basic();
  void test_enable();
  void test_close();

  void test_tracker_flags();
  void test_find_url();
  void test_can_scrape();

  void test_single_success();
  void test_single_failure();
  void test_single_closing();

  void test_multiple_success();

  void test_scrape_success();
  void test_scrape_failure();

  void test_has_active();
};

bool check_has_active_in_group(const torrent::TrackerList* tracker_list, const char* states, bool scrape);

#define TRACKER_LIST_SETUP()                                            \
  SETUP_THREAD_TRACKER();                                               \
                                                                        \
  torrent::TrackerList tracker_list;                                    \
  int success_counter = 0;                                              \
  int failure_counter = 0;                                              \
  int scrape_success_counter = 0;                                       \
  int scrape_failure_counter = 0;                                       \
                                                                        \
  tracker_list.slot_success() = std::bind(&increment_value_uint, &success_counter); \
  tracker_list.slot_failure() = std::bind(&increment_value_void, &failure_counter); \
  tracker_list.slot_scrape_success() = std::bind(&increment_value_void, &scrape_success_counter); \
  tracker_list.slot_scrape_failure() = std::bind(&increment_value_void, &scrape_failure_counter);

#define TRACKER_LIST_CLEANUP()                  \
  CLEANUP_THREAD_TRACKER();

#define TRACKER_INSERT(group, name)                                     \
  auto name = TrackerTest::new_tracker(&tracker_list, "");              \
  TrackerTest::insert_tracker(&tracker_list, group, name);

#define TEST_TRACKER_IS_BUSY(tracker, state)            \
  CPPUNIT_ASSERT(state == '0' ||  tracker.is_busy());   \
  CPPUNIT_ASSERT(state == '1' || !tracker.is_busy());

#define TEST_MULTI3_IS_BUSY(original, rearranged)                 \
  TEST_TRACKER_IS_BUSY(tracker_0_0, original[0]);                 \
  TEST_TRACKER_IS_BUSY(tracker_0_1, original[1]);                 \
  TEST_TRACKER_IS_BUSY(tracker_1_0, original[2]);                 \
  TEST_TRACKER_IS_BUSY(tracker_2_0, original[3]);                 \
  TEST_TRACKER_IS_BUSY(tracker_3_0, original[4]);                 \
  TEST_TRACKER_IS_BUSY(tracker_list[0], rearranged[0]);           \
  TEST_TRACKER_IS_BUSY(tracker_list[1], rearranged[1]);           \
  TEST_TRACKER_IS_BUSY(tracker_list[2], rearranged[2]);           \
  TEST_TRACKER_IS_BUSY(tracker_list[3], rearranged[3]);           \
  TEST_TRACKER_IS_BUSY(tracker_list[4], rearranged[4]);

#define TEST_GROUP_IS_BUSY(original, rearranged)                  \
  TEST_TRACKER_IS_BUSY(tracker_0_0, original[0]);                 \
  TEST_TRACKER_IS_BUSY(tracker_0_1, original[1]);                 \
  TEST_TRACKER_IS_BUSY(tracker_0_2, original[2]);                 \
  TEST_TRACKER_IS_BUSY(tracker_1_0, original[3]);                 \
  TEST_TRACKER_IS_BUSY(tracker_1_1, original[4]);                 \
  TEST_TRACKER_IS_BUSY(tracker_2_0, original[5]);                 \
  TEST_TRACKER_IS_BUSY(tracker_list[0], rearranged[0]);           \
  TEST_TRACKER_IS_BUSY(tracker_list[1], rearranged[1]);           \
  TEST_TRACKER_IS_BUSY(tracker_list[2], rearranged[2]);           \
  TEST_TRACKER_IS_BUSY(tracker_list[3], rearranged[3]);           \
  TEST_TRACKER_IS_BUSY(tracker_list[4], rearranged[4]);           \
  TEST_TRACKER_IS_BUSY(tracker_list[5], rearranged[5]);

#define TEST_TRACKERS_IS_BUSY_5(original, rearranged)   \
  TEST_TRACKER_IS_BUSY(tracker_0, original[0]);         \
  TEST_TRACKER_IS_BUSY(tracker_1, original[1]);         \
  TEST_TRACKER_IS_BUSY(tracker_2, original[2]);         \
  TEST_TRACKER_IS_BUSY(tracker_3, original[3]);         \
  TEST_TRACKER_IS_BUSY(tracker_4, original[4]);         \
  TEST_TRACKER_IS_BUSY(tracker_list[0], rearranged[0]); \
  TEST_TRACKER_IS_BUSY(tracker_list[1], rearranged[1]); \
  TEST_TRACKER_IS_BUSY(tracker_list[2], rearranged[2]); \
  TEST_TRACKER_IS_BUSY(tracker_list[3], rearranged[3]); \
  TEST_TRACKER_IS_BUSY(tracker_list[4], rearranged[4]);
