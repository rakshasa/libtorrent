#include "helpers/test_fixture.h"

class test_protocol_buffer : public test_fixture {
  CPPUNIT_TEST_SUITE(test_protocol_buffer);

  CPPUNIT_TEST(test_consume_within_range);
  CPPUNIT_TEST(test_consume_past_end_is_rejected);
  CPPUNIT_TEST(test_consume_negative_is_rejected);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_consume_within_range();
  void test_consume_past_end_is_rejected();
  void test_consume_negative_is_rejected();
};
