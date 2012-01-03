#include <cppunit/extensions/HelperMacros.h>

#include "data/hash_check_queue.h"

class HashCheckQueueTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(HashCheckQueueTest);
  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_single);

  CPPUNIT_TEST(test_thread);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_basic();
  void test_single();

  void test_thread();
};

