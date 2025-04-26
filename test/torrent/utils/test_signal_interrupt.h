#ifndef TEST_TORRENT_UTILS_SIGNAL_INTERRUPT_H
#define TEST_TORRENT_UTILS_SIGNAL_INTERRUPT_H

#include "test/helpers/test_fixture.h"

class TestSignalInterrupt : public test_fixture {
  CPPUNIT_TEST_SUITE(TestSignalInterrupt);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_thread_interrupt);
  CPPUNIT_TEST(test_latency);
  CPPUNIT_TEST(test_hammer);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();
  void test_thread_interrupt();
  void test_latency();
  void test_hammer();
};

#endif // TEST_TORRENT_UTILS_SIGNAL_INTERRUPT_H
