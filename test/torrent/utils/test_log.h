#include "helpers/test_fixture.h"

class test_log : public test_fixture {
  CPPUNIT_TEST_SUITE(test_log);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_output_open);

  CPPUNIT_TEST(test_print);
  CPPUNIT_TEST(test_children);
  CPPUNIT_TEST(test_file_output);
  CPPUNIT_TEST(test_file_output_append);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  static void test_basic();
  static void test_output_open();

  static void test_print();
  static void test_children();
  static void test_file_output();
  static void test_file_output_append();
};
