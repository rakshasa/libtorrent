#include "helpers/test_fixture.h"

class test_queue_buckets : public test_fixture {
  CPPUNIT_TEST_SUITE(test_queue_buckets);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_erase);
  CPPUNIT_TEST(test_find);

  CPPUNIT_TEST(test_destroy_range);
  CPPUNIT_TEST(test_move_range);

  CPPUNIT_TEST_SUITE_END();

public:
  static void test_basic();
  static void test_erase();
  static void test_find();

  static void test_destroy_range();
  static void test_move_range();
};
