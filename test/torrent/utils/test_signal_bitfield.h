#include "helpers/test_fixture.h"

class test_signal_bitfield : public test_fixture {
  CPPUNIT_TEST_SUITE(test_signal_bitfield);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_single);
  CPPUNIT_TEST(test_multiple);

  CPPUNIT_TEST(test_threaded);

  CPPUNIT_TEST_SUITE_END();

public:
  void tearDown();

  void test_basic();
  void test_single();
  void test_multiple();

  void test_threaded();
};
