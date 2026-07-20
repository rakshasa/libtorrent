#include "helpers/test_fixture.h"

class test_fd : public test_fixture {
  CPPUNIT_TEST_SUITE(test_fd);

  CPPUNIT_TEST(test_valid_flags);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_valid_flags();
};
