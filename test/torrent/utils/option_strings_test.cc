#include "config.h"

#include <fstream>
#include <iostream>
#include <tr1/functional>
#include <torrent/exceptions.h>
#include <torrent/utils/option_strings.h>

#include <torrent/connection_manager.h>
#include <torrent/object.h>
#include <torrent/download.h>
#include <torrent/download/choke_group.h>
#include <torrent/download/choke_queue.h>
#include <torrent/utils/log.h>

#include "option_strings_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(option_strings_test);

namespace tr1 { using namespace std::tr1; }

void
option_strings_test::test_basic() {
  
}

#define TEST_ENTRY(group, name, value)                                  \
  { std::string result(torrent::option_as_string(torrent::group, value)); \
    CPPUNIT_ASSERT_MESSAGE("Not found '" + result + "'", result == name); \
    CPPUNIT_ASSERT(torrent::option_find_string(torrent::group, name) == value); }

void
option_strings_test::test_entries() {
  TEST_ENTRY(OPTION_CONNECTION_TYPE, "leech", torrent::Download::CONNECTION_LEECH);
  TEST_ENTRY(OPTION_CONNECTION_TYPE, "seed", torrent::Download::CONNECTION_SEED);
  TEST_ENTRY(OPTION_CONNECTION_TYPE, "initial_seed", torrent::Download::CONNECTION_INITIAL_SEED);
  TEST_ENTRY(OPTION_CONNECTION_TYPE, "metadata", torrent::Download::CONNECTION_METADATA);

  TEST_ENTRY(OPTION_LOG_GROUP, "critical", torrent::LOG_CRITICAL);
  TEST_ENTRY(OPTION_LOG_GROUP, "storage_notice", torrent::LOG_STORAGE_NOTICE);
  TEST_ENTRY(OPTION_LOG_GROUP, "torrent_debug", torrent::LOG_TORRENT_DEBUG);
}
