#include <cppunit/extensions/HelperMacros.h>

#include "protocol/request_list.h"

class TestSocketAddressKey : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(TestSocketAddressKey);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();
};
