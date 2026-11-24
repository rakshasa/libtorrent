#include "config.h"

#include "test_limited_vector.h"

#include <functional>
#include <vector>

#include <torrent/utils/limited_vector.h>

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_limited_vector, "torrent/utils");

void
test_limited_vector::test_insert_sorted() {
  torrent::utils::limited_vector<int> values(5);

  CPPUNIT_ASSERT(values.limit() == 5);
  CPPUNIT_ASSERT(*values.insert_sorted(4) == 4);
  values.insert_sorted(1);
  values.insert_sorted(3);
  values.insert_sorted(3);
  values.insert_sorted(2);

  CPPUNIT_ASSERT(std::vector<int>(values.begin(), values.end()) == (std::vector<int>{ 4, 3, 3, 2, 1 }));
}

void
test_limited_vector::test_insert_bounded() {
  torrent::utils::limited_vector<int> values(3);
  values.insert_sorted(4);
  values.insert_sorted(1);
  values.insert_sorted(3);

  CPPUNIT_ASSERT(!values.insert_bounded(0));
  CPPUNIT_ASSERT(!values.insert_bounded(1));
  CPPUNIT_ASSERT(values.insert_bounded(3));
  CPPUNIT_ASSERT(values.insert_bounded(5));
  CPPUNIT_ASSERT(!values.insert_bounded(3));
  CPPUNIT_ASSERT(std::vector<int>(values.begin(), values.end()) == (std::vector<int>{ 5, 4, 3 }));

  values.clear();
  values.insert_sorted(7);
  values.insert_sorted(9);
  values.insert_sorted(8);
  CPPUNIT_ASSERT(values.insert_bounded(10));
  CPPUNIT_ASSERT(std::vector<int>(values.begin(), values.end()) == (std::vector<int>{ 10, 9, 8 }));
}

void
test_limited_vector::test_custom_compare() {
  torrent::utils::limited_vector<int, std::less<int>> values(3);
  values.insert_sorted(4);
  values.insert_sorted(1);
  values.insert_sorted(3);

  CPPUNIT_ASSERT(!values.insert_bounded(5));
  CPPUNIT_ASSERT(values.insert_bounded(2));
  CPPUNIT_ASSERT(values.insert_bounded(0));
  CPPUNIT_ASSERT(std::vector<int>(values.begin(), values.end()) == (std::vector<int>{ 0, 1, 2 }));
}

void
test_limited_vector::test_single_element() {
  torrent::utils::limited_vector<int> values(1);
  values.insert_sorted(3);

  CPPUNIT_ASSERT(!values.insert_bounded(2));
  CPPUNIT_ASSERT(!values.insert_bounded(3));
  CPPUNIT_ASSERT(values.insert_bounded(4));
  CPPUNIT_ASSERT_EQUAL(4, values.front());
  CPPUNIT_ASSERT_EQUAL(values.limit(), values.size());
}
