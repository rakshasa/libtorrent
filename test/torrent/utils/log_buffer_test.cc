#include "config.h"

#include <torrent/utils/log_buffer.h>

#include "globals.h"
#include "log_buffer_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(utils_log_buffer_test);

namespace tr1 { using namespace std::tr1; }

void
utils_log_buffer_test::setUp() {
  torrent::cachedTime = rak::timer::from_seconds(1000);
}

void
utils_log_buffer_test::tearDown() {
}

void
utils_log_buffer_test::test_basic() {
  torrent::log_buffer log;

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
utils_log_buffer_test::test_timestamps() {
  torrent::log_buffer log;

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
