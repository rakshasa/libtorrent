#include "helpers/test_fixture.h"

class test_extents : public test_fixture {
  CPPUNIT_TEST_SUITE(test_extents);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();
};
