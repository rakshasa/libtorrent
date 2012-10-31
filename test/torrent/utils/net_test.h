#include <cppunit/extensions/HelperMacros.h>

#include "torrent/utils/net.h"

class utils_net_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(utils_net_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
};
