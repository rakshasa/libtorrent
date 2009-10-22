#include <cppunit/extensions/HelperMacros.h>

#include "torrent/object_stream.h"

class ObjectStreamTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(ObjectStreamTest);
  CPPUNIT_TEST(testInputOrdered);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void testInputOrdered();
};

