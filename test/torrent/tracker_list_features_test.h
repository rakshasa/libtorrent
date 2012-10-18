#include <cppunit/extensions/HelperMacros.h>

class tracker_list_features_test : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(tracker_list_features_test);
  CPPUNIT_TEST(test_new_peers);
  CPPUNIT_TEST(test_has_active);
  CPPUNIT_TEST(test_find_next_to_request);
  CPPUNIT_TEST(test_find_next_to_request_groups);
  CPPUNIT_TEST(test_count_active);

  CPPUNIT_TEST(test_request_safeguard);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();
  void tearDown();

  void test_new_peers();
  void test_has_active();
  void test_find_next_to_request();
  void test_find_next_to_request_groups();
  void test_count_active();

  void test_request_safeguard();
};
