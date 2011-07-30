#include <cppunit/extensions/HelperMacros.h>

#include <vector>

#include "torrent/utils/ranges.h"

class RangesTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(RangesTest);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_intersect);
  CPPUNIT_TEST(test_create_union);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();
  void test_intersect();

  void test_create_union();
};
