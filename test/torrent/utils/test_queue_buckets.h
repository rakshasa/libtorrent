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
  void test_basic();
  void test_erase();
  void test_find();

  void test_destroy_range();
  void test_move_range();
};
