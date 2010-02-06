#include <cppunit/extensions/HelperMacros.h>

#include "torrent/object.h"

class ObjectTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(ObjectTest);
  CPPUNIT_TEST(testFlags);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void testFlags();
  void testMerge();
};

