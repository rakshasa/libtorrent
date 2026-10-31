#include "helpers/test_fixture.h"

class test_download_wrapper : public test_fixture {
  CPPUNIT_TEST_SUITE(test_download_wrapper);

  CPPUNIT_TEST(test_window_range);
  CPPUNIT_TEST(test_window_range_clear);
  CPPUNIT_TEST(test_window_range_cap);
  CPPUNIT_TEST(test_window_range_not_normal);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_window_range();
  void test_window_range_clear();
  void test_window_range_cap();
  void test_window_range_not_normal();
};
