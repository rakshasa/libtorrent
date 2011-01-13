#include "config.h"

#import "ranges_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(RangesTest);

void
RangesTest::test_basic() {
}

void
RangesTest::test_intersect() {
  rak::ranges<int> range;
  
  CPPUNIT_ASSERT(range.intersect_distance(0, 0) == 0);
  CPPUNIT_ASSERT(range.intersect_distance(0, 10) == 0);

  range.insert(0, 5);
  
  CPPUNIT_ASSERT(range.intersect_distance(0, 5) == 5);
  CPPUNIT_ASSERT(range.intersect_distance(0, 10) == 5);
  CPPUNIT_ASSERT(range.intersect_distance(-5, 5) == 5);
  CPPUNIT_ASSERT(range.intersect_distance(-5, 10) == 5);

  CPPUNIT_ASSERT(range.intersect_distance(2, 2) == 0);
  CPPUNIT_ASSERT(range.intersect_distance(1, 4) == 3);
  CPPUNIT_ASSERT(range.intersect_distance(1, 10) == 4);
  CPPUNIT_ASSERT(range.intersect_distance(-5, 4) == 4);

  range.insert(10, 15);

  CPPUNIT_ASSERT(range.intersect_distance(0, 15) == 10);
  CPPUNIT_ASSERT(range.intersect_distance(0, 20) == 10);
  CPPUNIT_ASSERT(range.intersect_distance(-5, 15) == 10);
  CPPUNIT_ASSERT(range.intersect_distance(-5, 20) == 10);

  CPPUNIT_ASSERT(range.intersect_distance(2, 12) == 5);
  CPPUNIT_ASSERT(range.intersect_distance(1, 14) == 8);
  CPPUNIT_ASSERT(range.intersect_distance(1, 20) == 9);
  CPPUNIT_ASSERT(range.intersect_distance(-5, 14) == 9);
}
