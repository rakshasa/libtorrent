#ifndef TEST_TORRENT_UTILS_SIGNAL_INTERRUPT_H
#define TEST_TORRENT_UTILS_SIGNAL_INTERRUPT_H

#include "helpers/test_fixture.h"

class TestSignalInterrupt : public test_fixture {
  CPPUNIT_TEST_SUITE(TestSignalInterrupt);

  CPPUNIT_TEST(test_basic);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();
};

#endif // TEST_TORRENT_UTILS_SIGNAL_INTERRUPT_H
