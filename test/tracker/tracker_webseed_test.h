#include <cppunit/extensions/HelperMacros.h>

#include "tracker/tracker_webseed.h"

class TrackerWebseedTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(TrackerWebseedTest);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
};
