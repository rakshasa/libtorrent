#include "config.h"

#include "test_sorted_vector.h"

#include <functional>
#include <vector>

#include <torrent/utils/sorted_vector.h>

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_sorted_vector, "torrent/utils");

void
test_sorted_vector::test_insert_sorted() {
  torrent::utils::sorted_vector<int> values;

  values.insert(4);
  values.insert(1);
  values.insert(3);
  values.insert(3);
  values.insert(2);

  CPPUNIT_ASSERT(std::vector<int>(values.begin(), values.end()) == (std::vector<int>{ 1, 2, 3, 3, 4 }));
}

void
test_sorted_vector::test_insert_bounded() {
  torrent::utils::sorted_vector<int> values;

  CPPUNIT_ASSERT(values.insert_bounded(4, 3));
  CPPUNIT_ASSERT(values.insert_bounded(1, 3));
  CPPUNIT_ASSERT(values.insert_bounded(3, 3));
  CPPUNIT_ASSERT(!values.insert_bounded(5, 3));
  CPPUNIT_ASSERT(values.insert_bounded(3, 3));
  CPPUNIT_ASSERT(values.insert_bounded(2, 3));
  CPPUNIT_ASSERT(!values.insert_bounded(3, 3));
  CPPUNIT_ASSERT(std::vector<int>(values.begin(), values.end()) == (std::vector<int>{ 1, 2, 3 }));
  CPPUNIT_ASSERT(!values.insert_bounded(0, 0));

  torrent::utils::sorted_vector<int, std::greater<int>> descending;

  CPPUNIT_ASSERT(descending.insert_bounded(3, 2));
  CPPUNIT_ASSERT(descending.insert_bounded(1, 2));
  CPPUNIT_ASSERT(descending.insert_bounded(5, 2));
  CPPUNIT_ASSERT(std::vector<int>(descending.begin(), descending.end()) == (std::vector<int>{ 5, 3 }));
}
