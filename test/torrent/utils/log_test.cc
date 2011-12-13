#include "config.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <tr1/functional>
#include <torrent/exceptions.h>
#include <torrent/utils/log.h>

#include "log_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(utils_log_test);

namespace tr1 { using namespace std::tr1; }

const char* expected_output = NULL;
unsigned int output_mask;

static void
test_output(const char* output, unsigned int length, unsigned int mask) {
  CPPUNIT_ASSERT_MESSAGE("'" + std::string(output) + "' != '" + std::string(expected_output) + "'",
                         std::strcmp(output, expected_output) == 0);
  CPPUNIT_ASSERT_MESSAGE("'" + std::string(output) + "'", std::strlen(output) == length);
  output_mask |= mask;
}

#define LTUNIT_ASSERT_OUTPUT(group, mask, expected, ...) \
  output_mask = 0; expected_output = expected;           \
  lt_log_print(group, __VA_ARGS__);                      \
  CPPUNIT_ASSERT(output_mask == (mask));

void
utils_log_test::setUp() {
  // Don't initialize since this creates the group->child connections.
  //  torrent::log_initialize();
}

void
utils_log_test::tearDown() {
  torrent::log_cleanup();
}

void
utils_log_test::test_basic() {
  CPPUNIT_ASSERT(!torrent::log_groups.empty());
  CPPUNIT_ASSERT(torrent::log_groups.size() == torrent::LOG_GROUP_MAX_SIZE);

  CPPUNIT_ASSERT(std::find_if(torrent::log_groups.begin(), torrent::log_groups.end(),
                              tr1::bind(&torrent::log_group::valid, tr1::placeholders::_1)) == torrent::log_groups.end());
}

inline void
open_output(const char* name, int mask = 0) {
  torrent::log_open_output(name, tr1::bind(&::test_output, tr1::placeholders::_1, tr1::placeholders::_2, mask));
}

void
utils_log_test::test_output_open() {
  CPPUNIT_ASSERT(torrent::log_groups[0].size_outputs() == 0);

  // Add test for unknown output names.

  open_output("test_output_1", 0x0);
  torrent::log_add_group_output(0, "test_output_1");

  CPPUNIT_ASSERT(torrent::log_outputs.size() == 1);
  CPPUNIT_ASSERT(torrent::log_outputs[0].first == "test_output_1");
  CPPUNIT_ASSERT(torrent::log_groups[0].outputs() == 0x1);
  CPPUNIT_ASSERT(torrent::log_groups[0].size_outputs() == 1);

  // Test inserting duplicate names, should catch.
  // CPPUNIT_ASSERT_THROW(torrent::log_open_output("test_output_1", torrent::log_slot());, torrent::input_error);

  try {
    torrent::log_open_output("test_output_1", torrent::log_slot());
  } catch (torrent::input_error& e) {
    return;
  }

  CPPUNIT_ASSERT(false);

  // Test more than 64 entries.
}

// Test to make sure we don't call functions when using lt_log_print
// on unused log items.

void
utils_log_test::test_print() {
  open_output("test_print_1", 0x1);
  open_output("test_print_2", 0x2);
  torrent::log_add_group_output(0, "test_print_1");
  
  LTUNIT_ASSERT_OUTPUT(0, 0x1, "foo_bar", "foo_bar");
  LTUNIT_ASSERT_OUTPUT(0, 0x1, "foo 123 bar", "foo %i %s", 123, "bar");

  torrent::log_add_group_output(0, "test_print_2");
  
  LTUNIT_ASSERT_OUTPUT(0, 0x1|0x2, "test_multiple", "test_multiple");
}

enum {
  GROUP_PARENT_1,
  GROUP_PARENT_2,
  GROUP_CHILD_1,
  GROUP_CHILD_1_1
};

void
utils_log_test::test_children() {
  open_output("test_children_1", 0x1);
  open_output("test_children_2", 0x2);
  torrent::log_add_group_output(GROUP_PARENT_1, "test_children_1");
  torrent::log_add_group_output(GROUP_PARENT_2, "test_children_2");

  torrent::log_add_child(GROUP_PARENT_1, GROUP_CHILD_1);
  torrent::log_add_child(GROUP_CHILD_1,  GROUP_CHILD_1_1);

  // std::cout << "cached_output(" << torrent::log_groups[GROUP_PARENT_1].cached_outputs() << ')';

  LTUNIT_ASSERT_OUTPUT(GROUP_PARENT_1,  0x1, "parent_1", "parent_1");
  LTUNIT_ASSERT_OUTPUT(GROUP_CHILD_1,   0x1, "child_1", "child_1");
  LTUNIT_ASSERT_OUTPUT(GROUP_CHILD_1_1, 0x1, "child_1", "child_1");

  torrent::log_add_child(GROUP_PARENT_2, GROUP_CHILD_1);

  LTUNIT_ASSERT_OUTPUT(GROUP_PARENT_2,  0x2, "parent_2", "parent_2");
  LTUNIT_ASSERT_OUTPUT(GROUP_CHILD_1,   0x3, "child_1", "child_1");
  LTUNIT_ASSERT_OUTPUT(GROUP_CHILD_1_1, 0x3, "child_1", "child_1");
}

void
utils_log_test::test_file_output() {
  char* filename = tmpnam(NULL);

  torrent::log_open_file_output("test_file", filename);
  torrent::log_add_group_output(GROUP_PARENT_1, "test_file");
  
  lt_log_print(GROUP_PARENT_1, "test_file");

  torrent::log_cleanup(); // To ensure we flush the buffers.

  std::ifstream temp_file(filename);

  CPPUNIT_ASSERT(temp_file.good());
  
  char buffer[256];
  temp_file.getline(buffer, 256);

  CPPUNIT_ASSERT_MESSAGE(buffer, std::string(buffer).find("test_file") != std::string::npos);
}
