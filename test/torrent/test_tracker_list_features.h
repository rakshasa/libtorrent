#include "test/helpers/test_fixture.h"

class test_tracker_list_features : public test_fixture {
  CPPUNIT_TEST_SUITE(test_tracker_list_features);

  CPPUNIT_TEST(test_new_peers);
  CPPUNIT_TEST(test_has_active);
  CPPUNIT_TEST(test_find_next_to_request);
  CPPUNIT_TEST(test_find_next_to_request_groups);
  CPPUNIT_TEST(test_count_active);

  CPPUNIT_TEST(test_request_safeguard);

  CPPUNIT_TEST_SUITE_END();

public:
  void setUp();

  void test_new_peers();
  void test_has_active();
  void test_find_next_to_request();
  void test_find_next_to_request_groups();
  void test_count_active();

  void test_request_safeguard();
};
