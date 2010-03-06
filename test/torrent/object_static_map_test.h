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

  void test_basics();
  void test_write();

  void test_read();

  void test_read_extensions();

  // Proper unit tests:
  void test_read_empty();
  void test_read_single();
  void test_read_single_raw();
  void test_read_raw_types();
  void test_read_multiple();
  void test_read_dict();

  void test_write_empty();
  void test_write_single();
  void test_write_multiple();
};

