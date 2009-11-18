#include <cppunit/extensions/HelperMacros.h>

#include "torrent/object_stream.h"

class ObjectStreamTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(ObjectStreamTest);
  CPPUNIT_TEST(testInputOrdered);
  CPPUNIT_TEST(testInputNullKey);
  CPPUNIT_TEST(testBuffer);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void testInputOrdered();
  void testInputNullKey();
  void testBuffer();
};

