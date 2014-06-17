#include <cppunit/extensions/HelperMacros.h>

#include "torrent/utils/directory_events.h"

class utils_directory_events_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(utils_directory_events_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
};
