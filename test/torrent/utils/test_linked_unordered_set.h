#include "helpers/test_fixture.h"

class test_linked_unordered_set : public test_fixture {
  CPPUNIT_TEST_SUITE(test_linked_unordered_set);

  CPPUNIT_TEST(test_iterate);
  CPPUNIT_TEST(test_iterator_after_rehash);
  CPPUNIT_TEST(test_erase_iterator);
  CPPUNIT_TEST(test_back);
  CPPUNIT_TEST(test_find);
  CPPUNIT_TEST(test_find_transparent);

  CPPUNIT_TEST_SUITE_END();

public:
  void test_iterate();
  void test_iterator_after_rehash();
  void test_erase_iterator();
  void test_back();
  void test_find();
  void test_find_transparent();
};
