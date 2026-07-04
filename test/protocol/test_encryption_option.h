#include "helpers/test_fixture.h"

class test_encryption_option : public test_fixture {
  CPPUNIT_TEST_SUITE(test_encryption_option);
  CPPUNIT_TEST(test_encryption_helpers);
  CPPUNIT_TEST_SUITE_END();

public:
  void test_encryption_helpers();
};