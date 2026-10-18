#include "helpers/test_fixture.h"

class test_string_utf8 : public test_fixture {
  CPPUNIT_TEST_SUITE(test_string_utf8);

  CPPUNIT_TEST(test_ascii);
  CPPUNIT_TEST(test_multibyte);
  CPPUNIT_TEST(test_tab_and_line_feed);
  CPPUNIT_TEST(test_carriage_return);
  CPPUNIT_TEST(test_c0_controls);
  CPPUNIT_TEST(test_c1_controls);
  CPPUNIT_TEST(test_invalid_utf8);
  CPPUNIT_TEST(test_is_valid_utf8_text);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_ascii();
  void test_multibyte();
  void test_tab_and_line_feed();
  void test_carriage_return();
  void test_c0_controls();
  void test_c1_controls();
  void test_invalid_utf8();
  void test_is_valid_utf8_text();
};
