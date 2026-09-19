#include "config.h"

#include "test_log_buffer.h"

#include <algorithm>

#include "torrent/utils/log.h"
#include "torrent/utils/log_buffer.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_log_buffer, "torrent/utils");

void
test_log_buffer::test_basic() {
  torrent::log_buffer log;
  m_main_thread->test_set_cached_time(1000s);

  log.lock();
  CPPUNIT_ASSERT(log.empty());
  CPPUNIT_ASSERT(log.find_older(0) == log.end());
  log.unlock();

  log.lock_and_push_log("foobar", 6, -1);
  CPPUNIT_ASSERT(log.empty());

  auto timestamp = std::chrono::seconds(365 * 24h + 1000s).count();

  log.lock_and_push_log("foobar", 6, 0);
  CPPUNIT_ASSERT(log.size() == 1);
  CPPUNIT_ASSERT(log.back().timestamp == timestamp);
  CPPUNIT_ASSERT(log.back().group == 0);
  CPPUNIT_ASSERT(log.back().message == "foobar");

  m_main_thread->test_add_cached_time(1s);

  log.lock_and_push_log("barbaz", 6, 0);
  CPPUNIT_ASSERT(log.size() == 2);
  CPPUNIT_ASSERT(log.back().timestamp == timestamp + 1);
  CPPUNIT_ASSERT(log.back().group == 0);
  CPPUNIT_ASSERT(log.back().message == "barbaz");
}

void
test_log_buffer::test_timestamps() {
  torrent::log_buffer log;
  m_main_thread->test_set_cached_time(1000s);

  auto timestamp = std::chrono::seconds(365 * 24h + 1000s).count();

  log.lock_and_push_log("foobar", 6, 0);
  CPPUNIT_ASSERT(log.back().timestamp == timestamp);
  CPPUNIT_ASSERT(log.find_older(timestamp - 1) == log.begin());
  CPPUNIT_ASSERT(log.find_older(timestamp)     == log.end());
  CPPUNIT_ASSERT(log.find_older(timestamp + 1) == log.end());

  m_main_thread->test_add_cached_time(10s);

  log.lock_and_push_log("foobar", 6, 0);
  CPPUNIT_ASSERT(log.back().timestamp == timestamp + 10);
  CPPUNIT_ASSERT(log.find_older(timestamp) == log.begin());
  CPPUNIT_ASSERT(log.find_older(timestamp + 10 - 1)  == log.begin() + 1);
  CPPUNIT_ASSERT(log.find_older(timestamp + 10)      == log.end());
  CPPUNIT_ASSERT(log.find_older(timestamp + 10 + 1)  == log.end());
}

// log_open_log_buffer() registers an output slot holding a raw pointer to the buffer, so dropping
// the buffer has to take the slot with it or the next log line writes into freed memory.
void
test_log_buffer::test_close_output_on_delete() {
  const std::string name = "test_log_buffer_lifetime";

  auto has_output = [&name]() {
    return std::any_of(torrent::log_outputs.begin(), torrent::log_outputs.end(),
                       [&name](const auto& output) { return output.first == name; });
  };

  auto outputs_before = torrent::log_outputs.size();

  {
    auto buffer = torrent::log_open_log_buffer(name.c_str());

    CPPUNIT_ASSERT_EQUAL(outputs_before + 1, torrent::log_outputs.size());
    CPPUNIT_ASSERT(has_output());
  }

  CPPUNIT_ASSERT(!has_output());
  CPPUNIT_ASSERT_EQUAL(outputs_before, torrent::log_outputs.size());
}
