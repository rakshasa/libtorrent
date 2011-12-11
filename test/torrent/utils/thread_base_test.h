#include <cppunit/extensions/HelperMacros.h>

#include "torrent/utils/thread_base.h"

class utils_thread_base_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(utils_thread_base_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
};
