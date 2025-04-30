#include "config.h"

#include "test_signal_interrupt.h"

#include <cstdlib>

#include "test/helpers/test_thread.h"
#include "test/helpers/network.h"
#include "torrent/exceptions.h"
#include "torrent/utils/thread.h"
#include "utils/signal_interrupt.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(TestSignalInterrupt, "torrent/utils");

void
TestSignalInterrupt::test_basic() {
  mock_redirect_defaults();

  auto pair = torrent::SignalInterrupt::create_pair();

  CPPUNIT_ASSERT(pair.first != nullptr);
  CPPUNIT_ASSERT(pair.second != nullptr);

  CPPUNIT_ASSERT(pair.first->file_descriptor() >= 0);
  CPPUNIT_ASSERT(pair.second->file_descriptor() >= 0);
  CPPUNIT_ASSERT(pair.first->file_descriptor() != pair.second->file_descriptor());

  for (int i = 0; i < 10; i++) {
    CPPUNIT_ASSERT(!pair.first->is_poking());
    CPPUNIT_ASSERT(!pair.second->is_poking());

    CPPUNIT_ASSERT(!check_event_is_readable(pair.second.get(), 0ms));
    CPPUNIT_ASSERT(!check_event_is_readable(pair.first.get(), 0ms));

    pair.first->poke();

    CPPUNIT_ASSERT(!pair.first->is_poking());
    CPPUNIT_ASSERT(pair.second->is_poking());

    CPPUNIT_ASSERT(check_event_is_readable(pair.second.get(), 1ms));
    CPPUNIT_ASSERT(!check_event_is_readable(pair.first.get(), 0ms));

    auto start_time = std::chrono::steady_clock::now();
    pair.second->event_read();
    auto end_time = std::chrono::steady_clock::now();

    CPPUNIT_ASSERT((end_time - start_time) < 1ms);
  }
}

// TODO: Add some allowance for loop_count checks not being ready after 1ms.

void
TestSignalInterrupt::test_thread_interrupt() {
  auto thread = test_thread::create();
  thread->set_test_flag(test_thread::test_flag_long_timeout);

  thread->init_thread();
  thread->start_thread();

  std::this_thread::sleep_for(10ms);
  CPPUNIT_ASSERT(thread->is_state(test_thread::STATE_ACTIVE));

  int loop_count = thread->loop_count();

  std::this_thread::sleep_for(500ms);
  CPPUNIT_ASSERT(thread->loop_count() == loop_count);

  thread->interrupt();

  std::this_thread::sleep_for(1ms);
  CPPUNIT_ASSERT(thread->loop_count() == loop_count + 2);
}

void
TestSignalInterrupt::test_latency() {
  auto thread = test_thread::create();
  thread->set_test_flag(test_thread::test_flag_long_timeout);

  thread->init_thread();
  thread->start_thread();

  std::this_thread::sleep_for(10ms);
  CPPUNIT_ASSERT(thread->is_state(test_thread::STATE_ACTIVE));

  int loop_count = thread->loop_count();

  auto start_time = std::chrono::steady_clock::now();

  for (int i = 0; i < 1000; i++) {
    thread->interrupt();

    for (int j = 0; j < 10; j++) {
      if (thread->loop_count() >= loop_count + 2)
        continue;

      std::this_thread::sleep_for(1ms);
    }

    auto new_count = thread->loop_count();
    CPPUNIT_ASSERT(new_count <= loop_count + 2);
    CPPUNIT_ASSERT(new_count == loop_count + 2);
    loop_count = new_count;
  }

  auto end_time = std::chrono::steady_clock::now();
  auto ignore_signal_latency = std::getenv("TEST_IGNORE_SIGNAL_INTERRUPT_LATENCY");

  if (ignore_signal_latency != nullptr && ignore_signal_latency == std::string("YES"))
    return;

  CPPUNIT_ASSERT((end_time - start_time) < 2s);
}

void
TestSignalInterrupt::test_hammer() {
  auto thread = test_thread::create();
  thread->set_test_flag(test_thread::test_flag_long_timeout);

  thread->init_thread();
  thread->start_thread();

  std::this_thread::sleep_for(10ms);
  CPPUNIT_ASSERT(thread->is_state(test_thread::STATE_ACTIVE));

  int loop_count = thread->loop_count();

  auto start_time = std::chrono::steady_clock::now();

  for (int i = 0; i < 1000; i++) {
    for (int j = 0; j < 10; j++)
      thread->interrupt();

    for (int j = 0; j < 10; j++) {
      if (thread->loop_count() >= loop_count + 2)
        continue;

      std::this_thread::sleep_for(1ms);
    }

    auto new_count = thread->loop_count();

    // Since we hammer the interrupt, we can't expect the loop count to just increment by 2.
    CPPUNIT_ASSERT(new_count <= loop_count + 20);

    loop_count = new_count;
  }

  auto end_time = std::chrono::steady_clock::now();
  auto ignore_signal_latency = std::getenv("TEST_IGNORE_SIGNAL_INTERRUPT_LATENCY");

  if (ignore_signal_latency != nullptr && ignore_signal_latency == std::string("YES"))
    return;

  CPPUNIT_ASSERT((end_time - start_time) < 2s);
}

// TODO: Test with high load / long process.
