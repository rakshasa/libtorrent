#include <cppunit/extensions/HelperMacros.h>

#include "data/hash_queue.h"

class HashQueueTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(HashQueueTest);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_single);
  CPPUNIT_TEST(test_multiple);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
  void test_single();
  void test_multiple();
};

