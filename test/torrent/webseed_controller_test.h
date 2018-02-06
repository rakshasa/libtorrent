#include <cppunit/extensions/HelperMacros.h>

class WebseedControllerTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(WebseedControllerTest);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
};
