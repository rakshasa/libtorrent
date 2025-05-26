#include "config.h"

#include "test_main_thread.h"

#include "test/helpers/mock_function.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/net/resolver.h"
#include "torrent/utils/log.h"
#include "torrent/utils/scheduler.h"

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

TestMainThread::TestMainThread() {}

TestMainThread::~TestMainThread() {
  m_self = nullptr;
}

void
TestMainThread::init_thread() {
  if (!torrent::Poll::slot_create_poll())
    throw torrent::internal_error("ThreadMain::init_thread(): Poll::slot_create_poll() not valid.");

  m_poll = std::unique_ptr<torrent::Poll>(torrent::Poll::slot_create_poll()());
  m_resolver = std::make_unique<torrent::net::Resolver>();
  m_state = STATE_INITIALIZED;

  //m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_MAIN - INSTRUMENTATION_POLLING_DO_POLL;

  init_thread_local();
}

void
TestMainThread::call_events() {
  process_callbacks();
}

std::chrono::microseconds
TestMainThread::next_timeout() {
  return std::chrono::microseconds(10min);
}

void
TestFixtureWithMainThread::setUp() {
  test_fixture::setUp();

  set_create_poll();

  m_main_thread = TestMainThread::create();
  m_main_thread->init_thread();
}

void
TestFixtureWithMainThread::tearDown() {
  m_main_thread.reset();

  test_fixture::tearDown();
}

void
TestFixtureWithMainAndTrackerThread::setUp() {
  test_fixture::setUp();

  set_create_poll();

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
  torrent::thread_tracker()->destroy_thread();

  m_main_thread.reset();
  test_fixture::tearDown();
}

void
TestFixtureWithMockAndMainThread::setUp() {
  test_fixture::setUp();

  set_create_poll();

  m_main_thread = TestMainThread::create_with_mock();
  m_main_thread->init_thread();
}

void
TestFixtureWithMockAndMainThread::tearDown() {
  m_main_thread.reset();
  test_fixture::tearDown();
}

