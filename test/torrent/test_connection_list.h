#include "helpers/test_fixture.h"

class test_connection_list : public test_fixture {
  CPPUNIT_TEST_SUITE(test_connection_list);

  CPPUNIT_TEST(test_basic);
  CPPUNIT_TEST(test_set_size);
  CPPUNIT_TEST(test_set_difference);
  CPPUNIT_TEST(test_erase_missing);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_basic();
  void test_set_size();
  void test_set_difference();
  void test_erase_missing();
};
