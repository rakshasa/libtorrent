#include "helpers/test_fixture.h"

class test_thread_base : public test_fixture {
  CPPUNIT_TEST_SUITE(test_thread_base);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_lifecycle);

  CPPUNIT_TEST(test_global_lock_basic);
  CPPUNIT_TEST(test_interrupt);
  CPPUNIT_TEST(test_stop);

  CPPUNIT_TEST_SUITE_END();

public:
  void tearDown();

  void test_basic();
  void test_lifecycle();

  void test_global_lock_basic();
  void test_interrupt();
  void test_interrupt_legacy();
  void test_stop();
};
