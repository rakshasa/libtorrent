#include "config.h"

#include "test/torrent/test_tracker_timeout.h"

#include "test/torrent/test_tracker_list.h"
#include "tracker/tracker_controller.h"

CPPUNIT_TEST_SUITE_REGISTRATION(TestTrackerTimeout);

void
TestTrackerTimeout::test_set_timeout() {
  auto tracker        = TrackerTest::new_tracker(nullptr, 0, "");
  auto tracker_worker = TrackerTest::test_worker(tracker);

  try {
    CPPUNIT_ASSERT(tracker.state().normal_interval() == torrent::tracker::TrackerState::min_normal_interval);
    CPPUNIT_ASSERT(tracker.state().min_interval()    == torrent::tracker::TrackerState::min_min_interval);

    tracker_worker->set_new_normal_interval(100);
    CPPUNIT_ASSERT(tracker.state().normal_interval() == 600);

    tracker_worker->set_new_normal_interval(8 * 4000);
    CPPUNIT_ASSERT(tracker.state().normal_interval() == 8 * 3600);

    tracker_worker->set_new_min_interval(100);
    CPPUNIT_ASSERT_EQUAL((uint32_t)300, tracker.state().min_interval());

    tracker_worker->set_new_min_interval(4 * 4000);
    CPPUNIT_ASSERT(tracker.state().min_interval() == 4 * 3600);

    tracker_worker->cleanup();

  } catch (const std::exception& e) {
    fprintf(stderr, "Exception in %s: %s\n", __func__, e.what());
    throw;
  }
}

void
TestTrackerTimeout::test_timeout_tracker() {
  auto tracker        = TrackerTest::new_tracker(nullptr, 0, "http://tracker.com/announce");
  auto tracker_worker = TrackerTest::test_worker(tracker);

  int flags = 0;

  try {
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 0);
    m_main_thread->test_add_cached_time(std::chrono::seconds(3));
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 0);

    flags = torrent::TrackerController::flag_active;

    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 0);
    tracker_worker->send_event(torrent::tracker::TrackerParams{}, torrent::tracker::TrackerState::EVENT_NONE);
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == ~uint32_t());
    tracker_worker->send_event(torrent::tracker::TrackerParams{}, torrent::tracker::TrackerState::EVENT_SCRAPE);
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 0);

    tracker_worker->close();
    tracker_worker->set_success(torrent::this_thread::cached_seconds().count());

    CPPUNIT_ASSERT_EQUAL((uint32_t)1800, torrent::tracker_next_timeout(tracker, flags));
    // CPPUNIT_ASSERT(1800 <= torrent::tracker_next_timeout(tracker, flags) && torrent::tracker_next_timeout(tracker, flags) <= 1800 + 3);
    tracker_worker->send_event(torrent::tracker::TrackerParams{}, torrent::tracker::TrackerState::EVENT_NONE);
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == ~uint32_t());
    tracker_worker->send_event(torrent::tracker::TrackerParams{}, torrent::tracker::TrackerState::EVENT_SCRAPE);
    CPPUNIT_ASSERT_EQUAL((uint32_t)1800, torrent::tracker_next_timeout(tracker, flags));
    // CPPUNIT_ASSERT(1800 <= torrent::tracker_next_timeout(tracker, flags) && torrent::tracker_next_timeout(tracker, flags) <= 1800 + 3);

    tracker_worker->close();

    tracker_worker->set_success(torrent::this_thread::cached_seconds().count() - 3);
    CPPUNIT_ASSERT_EQUAL((uint32_t)(1800 - 3), torrent::tracker_next_timeout(tracker, flags));
    tracker_worker->set_success(torrent::this_thread::cached_seconds().count() + 3);
    CPPUNIT_ASSERT_EQUAL((uint32_t)(1800 + 3), torrent::tracker_next_timeout(tracker, flags));

    tracker_worker->close();
    flags = torrent::TrackerController::flag_active | torrent::TrackerController::flag_promiscuous_mode;

    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 603);

    tracker_worker->send_event(torrent::tracker::TrackerParams{}, torrent::tracker::TrackerState::EVENT_NONE);
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == ~uint32_t());
    tracker_worker->send_event(torrent::tracker::TrackerParams{}, torrent::tracker::TrackerState::EVENT_SCRAPE);
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 603);

    tracker_worker->cleanup();

  } catch (const std::exception& e) {
    fprintf(stderr, "Exception in %s: %s\n", __func__, e.what());
    throw;
  }
}

void
TestTrackerTimeout::test_timeout_update() {
  auto tracker        = TrackerTest::new_tracker(nullptr, 0, "http://tracker.com/announce");
  auto tracker_worker = TrackerTest::test_worker(tracker);

  int flags = 0;

  flags = torrent::TrackerController::flag_active | torrent::TrackerController::flag_send_update;

  try {

    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 0);
    tracker_worker->send_event(torrent::tracker::TrackerParams{}, torrent::tracker::TrackerState::EVENT_SCRAPE);

    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 0);
    tracker_worker->send_event(torrent::tracker::TrackerParams{}, torrent::tracker::TrackerState::EVENT_NONE);

    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == ~uint32_t());

    tracker_worker->close();

    tracker_worker->set_failed(torrent::this_thread::cached_seconds().count());
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 5);

    tracker_worker->set_success(torrent::this_thread::cached_seconds().count());
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 600);

    tracker_worker->cleanup();

  } catch (const std::exception& e) {
    fprintf(stderr, "Exception in %s: %s\n", __func__, e.what());
    throw;
  }
}

void
TestTrackerTimeout::test_timeout_requesting() {
  auto tracker        = TrackerTest::new_tracker(nullptr, 0, "http://tracker.com/announce");
  auto tracker_worker = TrackerTest::test_worker(tracker);

  int flags = 0;

  flags = torrent::TrackerController::flag_active | torrent::TrackerController::flag_requesting;

  try {

    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 0);
    tracker_worker->send_event(torrent::tracker::TrackerParams{}, torrent::tracker::TrackerState::EVENT_SCRAPE);
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 0);
    tracker_worker->send_event(torrent::tracker::TrackerParams{}, torrent::tracker::TrackerState::EVENT_NONE);
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == ~uint32_t());

    // tracker_worker->set_latest_new_peers(10 - 1);

    tracker_worker->close();

    tracker_worker->set_failed(torrent::this_thread::cached_seconds().count());
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 5);

    tracker_worker->set_failed(torrent::this_thread::cached_seconds().count());
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 10);

    tracker_worker->set_failed(torrent::this_thread::cached_seconds().count());
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 20);

    tracker_worker->set_failed(torrent::this_thread::cached_seconds().count());
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 40);

    tracker_worker->set_failed(torrent::this_thread::cached_seconds().count());
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 80);

    tracker_worker->set_failed(torrent::this_thread::cached_seconds().count());
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 160);

    tracker_worker->set_failed(torrent::this_thread::cached_seconds().count());
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 300);

    //std::cout << "timeout:" << torrent::tracker_next_timeout(tracker, flags) << std::endl;

    tracker_worker->set_success(torrent::this_thread::cached_seconds().count());

    // CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 10);
    // tracker_worker->set_success(1, torrent::this_thread::cached_seconds().count());
    // CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 20);
    // tracker_worker->set_success(2, torrent::this_thread::cached_seconds().count());
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 600);
    tracker_worker->set_success(torrent::this_thread::cached_seconds().count());
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 600);

    tracker_worker->set_success(torrent::this_thread::cached_seconds().count());
    // tracker_worker->set_latest_sum_peers(9);
    // CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 20);

    tracker_worker->set_latest_sum_peers(10);
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 600);

    tracker_worker->set_latest_sum_peers(10);
    tracker_worker->set_latest_new_peers(10);
    tracker_worker->set_success(torrent::this_thread::cached_seconds().count());
    CPPUNIT_ASSERT(torrent::tracker_next_timeout(tracker, flags) == 600);

    tracker_worker->cleanup();

  } catch (const std::exception& e) {
    fprintf(stderr, "Exception in %s: %s\n", __func__, e.what());
    throw;
  }
}
