#include "config.h"

#include <iostream>
#include <torrent/utils/log.h>

#include "log_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(utils_log_test);

namespace std { using namespace tr1; }

void
utils_log_test::setUp() {
  torrent::log_initialize();
}

void
utils_log_test::tearDown() {
  torrent::log_cleanup();
}

void
utils_log_test::test_basic() {
  CPPUNIT_ASSERT(!torrent::log_groups.empty());
  CPPUNIT_ASSERT(torrent::log_groups.size() == torrent::LOG_MAX_SIZE);

  CPPUNIT_ASSERT(std::find_if(torrent::log_groups.begin(), torrent::log_groups.end(),
                              std::bind(&torrent::log_group::valid, std::placeholders::_1)) == torrent::log_groups.end());

  
}

// Test to make sure we don't call functions when using lt_log_print
// on unused log items.
