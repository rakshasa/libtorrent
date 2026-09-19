#include "helpers/test_fixture.h"

class test_chunk : public test_fixture {
  CPPUNIT_TEST_SUITE(test_chunk);

  CPPUNIT_TEST(test_buffer_rejects_wrapping_range);
  CPPUNIT_TEST(test_from_buffer_bus_error_keeps_signal_mask);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_buffer_rejects_wrapping_range();
  void test_from_buffer_bus_error_keeps_signal_mask();
};
