#include <cppunit/extensions/HelperMacros.h>

class UriParserTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(UriParserTest);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_basic_magnet);
  CPPUNIT_TEST(test_query_magnet);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
  void test_basic_magnet();
  void test_query_magnet();
};
