#include <cppunit/extensions/HelperMacros.h>

#include "torrent/utils/log.h"

class utils_log_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(utils_log_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
};
