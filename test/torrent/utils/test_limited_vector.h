#include "helpers/test_fixture.h"

class test_limited_vector : public test_fixture {
  CPPUNIT_TEST_SUITE(test_limited_vector);

  CPPUNIT_TEST(test_insert_sorted);
  CPPUNIT_TEST(test_insert_bounded);
  CPPUNIT_TEST(test_custom_compare);
  CPPUNIT_TEST(test_single_element);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_insert_sorted();
  void test_insert_bounded();
  void test_custom_compare();
  void test_single_element();
};
