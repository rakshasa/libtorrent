#include "helpers/test_fixture.h"

class test_file_list : public test_fixture {
  CPPUNIT_TEST_SUITE(test_file_list);

  CPPUNIT_TEST(test_initialize_rejects_max_u64);
  CPPUNIT_TEST(test_initialize_rejects_wrap_boundary);
  CPPUNIT_TEST(test_initialize_rejects_just_below_wrap_boundary);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_initialize_rejects_max_u64();
  void test_initialize_rejects_wrap_boundary();
  void test_initialize_rejects_just_below_wrap_boundary();
};
