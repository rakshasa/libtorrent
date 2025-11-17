#include "config.h"

#include "test_main_thread.h"

#include <signal.h>

#include "data/thread_disk.h"
#include "net/thread_net.h"
#include "test/helpers/mock_function.h"
#include "torrent/exceptions.h"
#include "torrent/net/resolver.h"
#include "torrent/utils/log.h"
#include "torrent/utils/scheduler.h"
#include "tracker/thread_tracker.h"

std::unique_ptr<TestMainThread>
TestMainThread::create() {
  // Needs to be called before Thread is created.
  mock_redirect_defaults();
  return std::unique_ptr<TestMainThread>(new TestMainThread());
}

std::unique_ptr<TestMainThread>
TestMainThread::create_with_mock() {
  return std::unique_ptr<TestMainThread>(new TestMainThread());
}

void
TestMainThread::init_thread() {
  m_resolver = std::make_unique<torrent::net::Resolver>();
  m_state = STATE_INITIALIZED;

  //m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_MAIN - INSTRUMENTATION_POLLING_DO_POLL;

  init_thread_local();
}

void
TestMainThread::cleanup_thread() {
}

void
TestMainThread::call_events() {
  process_callbacks();
}

std::chrono::microseconds
TestMainThread::next_timeout() {
  return 10min;
}

void
TestFixtureWithMainThread::setUp() {
  test_fixture::setUp();

  m_main_thread = TestMainThread::create();
  m_main_thread->init_thread();
}

void
TestFixtureWithMainThread::tearDown() {
  m_main_thread.reset();

  test_fixture::tearDown();
}

void
TestFixtureWithMainAndDiskThread::setUp() {
  test_fixture::setUp();

  m_main_thread = TestMainThread::create();
  m_main_thread->init_thread();

  // m_hash_check_queue.slot_chunk_done() binds to main_thread().

  torrent::ThreadDisk::create_thread();
  torrent::thread_disk()->init_thread();
  torrent::thread_disk()->start_thread();

  signal(SIGUSR1, [](auto){});
}

void
TestFixtureWithMainAndDiskThread::tearDown() {
  torrent::thread_disk()->stop_thread_wait();
  torrent::ThreadDisk::destroy_thread();

  m_main_thread.reset();

  test_fixture::tearDown();
}

void
TestFixtureWithMainAndTrackerThread::setUp() {
  test_fixture::setUp();

  m_main_thread = TestMainThread::create();
  m_main_thread->init_thread();

  log_add_group_output(torrent::LOG_TRACKER_EVENTS, "test_output");
  log_add_group_output(torrent::LOG_TRACKER_REQUESTS, "test_output");

  torrent::ThreadTracker::create_thread(m_main_thread.get());
  torrent::thread_tracker()->init_thread();
  torrent::thread_tracker()->start_thread();
}

void
TestFixtureWithMainAndTrackerThread::tearDown() {
  torrent::thread_tracker()->stop_thread_wait();
  torrent::ThreadTracker::destroy_thread();

  m_main_thread.reset();

  test_fixture::tearDown();
}

void
TestFixtureWithMainNetTrackerThread::setUp() {
  test_fixture::setUp();

  m_main_thread = TestMainThread::create();
  m_main_thread->init_thread();

  log_add_group_output(torrent::LOG_TRACKER_EVENTS, "test_output");
  log_add_group_output(torrent::LOG_TRACKER_REQUESTS, "test_output");

  torrent::ThreadNet::create_thread();
  torrent::ThreadTracker::create_thread(m_main_thread.get());

  torrent::net_thread::thread()->init_thread();
  torrent::thread_tracker()->init_thread();

  torrent::net_thread::thread()->start_thread();
  torrent::thread_tracker()->start_thread();
}

void
TestFixtureWithMainNetTrackerThread::tearDown() {
  torrent::thread_tracker()->stop_thread_wait();
  torrent::net_thread::thread()->stop_thread_wait();

  torrent::ThreadTracker::destroy_thread();
  torrent::ThreadNet::destroy_thread();

  m_main_thread.reset();

  test_fixture::tearDown();
}

void
TestFixtureWithMockAndMainThread::setUp() {
  test_fixture::setUp();

  m_main_thread = TestMainThread::create_with_mock();
  m_main_thread->init_thread();
}

void
TestFixtureWithMockAndMainThread::tearDown() {
  m_main_thread.reset();

  test_fixture::tearDown();
}

