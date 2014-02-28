#include <cppunit/extensions/HelperMacros.h>

#include "protocol/request_list.h"

class TestRequestList : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(TestRequestList);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_single_request);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();

  void test_single_request();
};
