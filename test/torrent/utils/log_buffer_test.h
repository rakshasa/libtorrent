#include <cppunit/extensions/HelperMacros.h>

#include "torrent/utils/log_buffer.h"

class utils_log_buffer_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(utils_log_buffer_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_timestamps);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
  void test_timestamps();
};
