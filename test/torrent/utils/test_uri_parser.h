#include "helpers/test_fixture.h"

class test_uri_parser : public test_fixture {
  CPPUNIT_TEST_SUITE(test_uri_parser);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_basic_magnet);
  CPPUNIT_TEST(test_query_magnet);

  CPPUNIT_TEST_SUITE_END();

public:
  static void test_basic();
  static void test_basic_magnet();
  static void test_query_magnet();
};
