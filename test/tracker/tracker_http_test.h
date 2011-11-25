#include <cppunit/extensions/HelperMacros.h>

#include "tracker/tracker_http.h"

class tracker_http_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(tracker_http_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_scrape);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();

  void test_scrape();
};
