#include <cppunit/extensions/HelperMacros.h>

class test_socket_address_key : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(test_socket_address_key);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_ordering);
  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();
  void test_ordering();
};
