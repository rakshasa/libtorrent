#include <cppunit/extensions/HelperMacros.h>

class ExtentsTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(ExtentsTest);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
};
