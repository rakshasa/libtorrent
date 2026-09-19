#include "helpers/test_fixture.h"

class test_chunk : public test_fixture {
  CPPUNIT_TEST_SUITE(test_chunk);

  CPPUNIT_TEST(test_buffer_rejects_wrapping_range);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_buffer_rejects_wrapping_range();
};
