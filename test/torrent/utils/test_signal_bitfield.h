#include "helpers/test_fixture.h"

class test_signal_bitfield : public test_fixture {
  CPPUNIT_TEST_SUITE(test_signal_bitfield);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_single);
  CPPUNIT_TEST(test_multiple);

  CPPUNIT_TEST(test_threaded);

  CPPUNIT_TEST_SUITE_END();

public:
  static void test_basic();
  static void test_single();
  static void test_multiple();

  static void test_threaded();
};
