#include "helpers/test_fixture.h"

class test_thread_base : public test_fixture {
  CPPUNIT_TEST_SUITE(test_thread_base);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_lifecycle);

  CPPUNIT_TEST(test_interrupt);
  CPPUNIT_TEST(test_stop);

  CPPUNIT_TEST_SUITE_END();

public:
  static void test_basic();
  static void test_lifecycle();

  static void test_interrupt();
  static void test_interrupt_legacy();
  static void test_stop();
};
