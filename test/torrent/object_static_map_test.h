#include <cppunit/extensions/HelperMacros.h>

#include "torrent/object.h"

class ObjectStaticMapTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(ObjectStaticMapTest);
  CPPUNIT_TEST(test_basics);
  CPPUNIT_TEST(test_write);
  CPPUNIT_TEST(test_read);
  CPPUNIT_TEST(test_read_extensions);

  CPPUNIT_TEST(test_read_empty);
  CPPUNIT_TEST(test_read_single);
  CPPUNIT_TEST(test_read_single_raw);
  CPPUNIT_TEST(test_read_raw_types);
  CPPUNIT_TEST(test_read_multiple);
  CPPUNIT_TEST(test_read_dict);

  CPPUNIT_TEST(test_write_empty);
  CPPUNIT_TEST(test_write_single);
  CPPUNIT_TEST(test_write_multiple);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  static void test_basics();
  static void test_write();

  static void test_read();

  static void test_read_extensions();

  // Proper unit tests:
  static void test_read_empty();
  static void test_read_single();
  static void test_read_single_raw();
  static void test_read_raw_types();
  static void test_read_multiple();
  static void test_read_dict();

  static void test_write_empty();
  static void test_write_single();
  static void test_write_multiple();
};

