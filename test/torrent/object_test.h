#include <cppunit/extensions/HelperMacros.h>

#include "torrent/object.h"

class ObjectTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(ObjectTest);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_flags);
  CPPUNIT_TEST(test_swap_and_move);

  CPPUNIT_TEST(test_create_normal);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}

  static void test_basic();
  static void test_flags();
  static void test_merge();

  static void test_swap_and_move();

  static void test_create_normal();
};

