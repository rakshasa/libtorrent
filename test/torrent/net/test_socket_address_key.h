#include <cppunit/extensions/HelperMacros.h>

#include "protocol/request_list.h"

class test_socket_address_key : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(test_socket_address_key);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();
};
