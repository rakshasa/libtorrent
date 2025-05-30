#include "helpers/test_main_thread.h"

class test_uri_parser : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(test_uri_parser);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_basic_magnet);
  CPPUNIT_TEST(test_query_magnet);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();
  void test_basic_magnet();
  void test_query_magnet();
};
