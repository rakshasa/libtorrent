#include "helpers/test_main_thread.h"

class test_hash_queue : public TestFixtureWithMainAndDiskThread {
  CPPUNIT_TEST_SUITE(test_hash_queue);

  CPPUNIT_TEST(test_single);
  CPPUNIT_TEST(test_multiple);
  CPPUNIT_TEST(test_erase);
  CPPUNIT_TEST(test_erase_stress);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_single();
  void test_multiple();
  void test_erase();
  void test_erase_stress();
};

