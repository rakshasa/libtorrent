#include "helpers/test_fixture.h"

class test_sorted_vector : public test_fixture {
  CPPUNIT_TEST_SUITE(test_sorted_vector);

  CPPUNIT_TEST(test_insert_sorted);
  CPPUNIT_TEST(test_insert_bounded);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_insert_sorted();
  void test_insert_bounded();
};
