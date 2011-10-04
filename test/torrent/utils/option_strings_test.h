#include <cppunit/extensions/HelperMacros.h>

#include "torrent/utils/option_strings.h"

class option_strings_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(option_strings_test);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_entries);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void test_basic();
  void test_entries();
};
