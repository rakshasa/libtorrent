#include "test/helpers/test_main_thread.h"

class TestPexAdded6 : public TestFixtureWithMainThread {
  CPPUNIT_TEST_SUITE(TestPexAdded6);

  CPPUNIT_TEST(test_valid_peers);
  CPPUNIT_TEST(test_unroutable_and_invalid_filtered);
  CPPUNIT_TEST(test_truncated_entry_ignored);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_valid_peers();
  void test_unroutable_and_invalid_filtered();
  void test_truncated_entry_ignored();
};
