#include "config.h"

#include "test_log_buffer.h"

#include "globals.h"
#include <torrent/utils/log_buffer.h>

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_log_buffer, "torrent/utils");

void
test_log_buffer::test_basic() {
  torrent::log_buffer log;
  torrent::cachedTime = rak::timer::from_seconds(1000);

  log.lock();
  CPPUNIT_ASSERT(log.empty());
  CPPUNIT_ASSERT(log.find_older(0) == log.end());
  log.unlock();

  log.lock_and_push_log("foobar", 6, -1);
  CPPUNIT_ASSERT(log.empty());

  log.lock_and_push_log("foobar", 6, 0);
  CPPUNIT_ASSERT(log.size() == 1);
  CPPUNIT_ASSERT(log.back().timestamp == 1000);
  CPPUNIT_ASSERT(log.back().group == 0);
  CPPUNIT_ASSERT(log.back().message == "foobar");

  torrent::cachedTime += rak::timer::from_milliseconds(1000);

  log.lock_and_push_log("barbaz", 6, 0);
  CPPUNIT_ASSERT(log.size() == 2);
  CPPUNIT_ASSERT(log.back().timestamp == 1001);
  CPPUNIT_ASSERT(log.back().group == 0);
  CPPUNIT_ASSERT(log.back().message == "barbaz");
}

void
test_log_buffer::test_timestamps() {
  torrent::log_buffer log;
  torrent::cachedTime = rak::timer::from_seconds(1000);

  log.lock_and_push_log("foobar", 6, 0);
  CPPUNIT_ASSERT(log.back().timestamp == 1000);
  CPPUNIT_ASSERT(log.find_older(1000 - 1) == log.begin());
  CPPUNIT_ASSERT(log.find_older(1000)     == log.end());
  CPPUNIT_ASSERT(log.find_older(1000 + 1) == log.end());

  torrent::cachedTime += rak::timer::from_milliseconds(10 * 1000);

  log.lock_and_push_log("foobar", 6, 0);
  CPPUNIT_ASSERT(log.back().timestamp == 1010);
  CPPUNIT_ASSERT(log.find_older(1010 - 10) == log.begin());
  CPPUNIT_ASSERT(log.find_older(1010 - 1)  == log.begin() + 1);
  CPPUNIT_ASSERT(log.find_older(1010)      == log.end());
  CPPUNIT_ASSERT(log.find_older(1010 + 1)  == log.end());
}
