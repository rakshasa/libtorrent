#include <cppunit/extensions/HelperMacros.h>

#include <vector>

#include "rak/ranges.h"

class RangesTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(RangesTest);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_intersect);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();
  void test_intersect();
};
