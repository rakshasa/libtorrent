#include <cppunit/extensions/HelperMacros.h>

class test_address_info : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(test_address_info);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_numericserv);
  CPPUNIT_TEST(test_helpers);

  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  static void test_basic();
  static void test_numericserv();
  static void test_helpers();
};
