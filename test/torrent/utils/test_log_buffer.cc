#include "config.h"

#include "test_log_buffer.h"

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
