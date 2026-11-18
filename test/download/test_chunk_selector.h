#include "helpers/test_fixture.h"

class test_chunk_selector : public test_fixture {
  CPPUNIT_TEST_SUITE(test_chunk_selector);

  CPPUNIT_TEST(test_ratio_default);
  CPPUNIT_TEST(test_window_ratio);
  CPPUNIT_TEST(test_window_ratio_picks);
  CPPUNIT_TEST(test_ratio_fallback);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_ratio_default();
  void test_window_ratio();
  void test_window_ratio_picks();
  void test_ratio_fallback();
};
