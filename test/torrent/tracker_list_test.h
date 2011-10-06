#include <cppunit/extensions/HelperMacros.h>

#include "torrent/tracker_list.h"

class tracker_list_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(tracker_list_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_single_tracker);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();
  void test_single_tracker();
};
