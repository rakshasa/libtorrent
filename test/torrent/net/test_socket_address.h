#include <cppunit/extensions/HelperMacros.h>

class test_socket_address : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(test_socket_address);
  CPPUNIT_TEST(test_make);
  CPPUNIT_TEST(test_sin_from_sa);
  CPPUNIT_TEST(test_sa_in_compare);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_make();
  void test_sin_from_sa();
  void test_sa_in_compare();
};
