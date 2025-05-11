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

  static void testInputOrdered();
  static void testInputNullKey();
  static void testOutputMask();
  static void testBuffer();

  static void testReadBencodeC();

  static void test_read_skip();
  static void test_read_skip_invalid();

  static void test_write();
};

