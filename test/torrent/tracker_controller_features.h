#include <cppunit/extensions/HelperMacros.h>

#include "tracker_controller_test.h"

class tracker_controller_features : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(tracker_controller_features);

  CPPUNIT_TEST(test_requesting_basic);
  CPPUNIT_TEST(test_requesting_timeout);
  CPPUNIT_TEST(test_promiscious_timeout);
  CPPUNIT_TEST(test_promiscious_failed);

  CPPUNIT_TEST(test_scrape_basic);
  CPPUNIT_TEST(test_scrape_priority);

  CPPUNIT_TEST(test_groups_requesting);
  CPPUNIT_TEST(test_groups_scrape);

  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_requesting_basic();
  void test_requesting_timeout();
  void test_promiscious_timeout();
  void test_promiscious_failed();

  void test_scrape_basic();
  void test_scrape_priority();

  void test_groups_requesting();
  void test_groups_scrape();
};
