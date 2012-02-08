#include <cppunit/extensions/HelperMacros.h>

#include "torrent/utils/signal_bitfield.h"

class utils_signal_bitfield_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(utils_signal_bitfield_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_single);
  CPPUNIT_TEST(test_multiple);

  CPPUNIT_TEST(test_thread);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
  void test_single();
  void test_multiple();

  void test_thread();
};
