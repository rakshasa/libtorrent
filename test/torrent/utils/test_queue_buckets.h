#include <cppunit/extensions/HelperMacros.h>

#include "protocol/request_list.h"

class TestQueueBuckets : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(TestQueueBuckets);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_erase);
  CPPUNIT_TEST(test_find);

  CPPUNIT_TEST(test_destroy_range);
  CPPUNIT_TEST(test_move_range);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();
  void test_erase();
  void test_find();

  void test_destroy_range();
  void test_move_range();
};
