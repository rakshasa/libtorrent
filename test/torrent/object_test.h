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

  void test_basic();
  void test_flags();
  void test_merge();

  void test_swap_and_move();

  void test_create_normal();
};

