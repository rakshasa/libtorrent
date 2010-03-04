#include <cppunit/extensions/HelperMacros.h>

#include "torrent/object_stream.h"

class ObjectStreamTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(ObjectStreamTest);
  CPPUNIT_TEST(testInputOrdered);
  CPPUNIT_TEST(testInputNullKey);
  CPPUNIT_TEST(testOutputMask);
  CPPUNIT_TEST(testBuffer);
  CPPUNIT_TEST(testReadBencodeC);

  CPPUNIT_TEST(test_read_skip);
  CPPUNIT_TEST(test_read_skip_invalid);
  CPPUNIT_TEST(test_write);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  void testInputOrdered();
  void testInputNullKey();
  void testOutputMask();
  void testBuffer();

  void testReadBencodeC();

  void test_read_skip();
  void test_read_skip_invalid();

  void test_write();
};

