#include "config.h"

#include "test_linked_unordered_set.h"

#include <iterator>
#include <memory>
#include <vector>

#include <torrent/utils/linked_unordered_set.h>

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_linked_unordered_set, "torrent/utils");

namespace {

using int_set = torrent::utils::linked_unordered_set<int>;

static_assert(std::forward_iterator<int_set::const_iterator>);

std::vector<int>
to_vector(const int_set& set) {
  return std::vector<int>(set.begin(), set.end());
}

struct entry {
  int id;
};

struct entry_hash {
  using is_transparent = void;

  size_t operator()(int id) const { return std::hash<int>{}(id); }
  size_t operator()(const std::shared_ptr<entry>& e) const { return (*this)(e->id); }
};

struct entry_equal {
  using is_transparent = void;

  bool operator()(int id, const std::shared_ptr<entry>& e) const { return id == e->id; }
  bool operator()(const std::shared_ptr<entry>& e, int id) const { return e->id == id; }
  bool operator()(const std::shared_ptr<entry>& lhs, const std::shared_ptr<entry>& rhs) const { return lhs->id == rhs->id; }
};

using entry_set = torrent::utils::linked_unordered_set<std::shared_ptr<entry>, entry_hash, entry_equal>;

} // namespace

void
test_linked_unordered_set::test_iterate() {
  int_set set;

  CPPUNIT_ASSERT(set.begin() == set.end());

  for (int i = 0; i < 5; i++)
    set.insert_back(i);

  set.insert_back(2);
  set.erase(0);
  set.erase(2);
  set.erase(4);
  set.insert_back(5);

  CPPUNIT_ASSERT(to_vector(set) == (std::vector<int>{ 1, 3, 5 }));

  auto itr  = set.begin();
  auto prev = itr++;

  CPPUNIT_ASSERT(*prev == 1);
  CPPUNIT_ASSERT(*itr == 3);
}

void
test_linked_unordered_set::test_iterator_after_rehash() {
  int_set set;

  for (int i = 0; i < 3; i++)
    set.insert_back(i);

  auto itr = std::next(set.begin());

  for (int i = 3; i < 1000; i++)
    set.insert_back(i);

  CPPUNIT_ASSERT(*itr == 1);

  ++itr;

  CPPUNIT_ASSERT(*itr == 2);

  for (int i = 3; i < 999; i++)
    set.erase(i);

  ++itr;

  CPPUNIT_ASSERT(*itr == 999);

  ++itr;

  CPPUNIT_ASSERT(itr == set.end());
}

void
test_linked_unordered_set::test_erase_iterator() {
  int_set set;

  for (int i = 0; i < 4; i++)
    set.insert_back(i);

  auto last = std::next(set.begin(), 3);

  auto itr = set.erase(set.begin());

  CPPUNIT_ASSERT(*itr == 1);

  itr = set.erase(std::next(set.begin()));

  CPPUNIT_ASSERT(*itr == 3);

  itr = set.erase(last);

  CPPUNIT_ASSERT(itr == set.end());
  CPPUNIT_ASSERT(set.size() == 1);
  CPPUNIT_ASSERT(to_vector(set) == (std::vector<int>{ 1 }));
}

void
test_linked_unordered_set::test_back() {
  int_set set;

  for (int i = 0; i < 3; i++)
    set.insert_back(i);

  CPPUNIT_ASSERT(set.back() == 2);

  int popped = set.pop_back();

  CPPUNIT_ASSERT(popped == 2);
  CPPUNIT_ASSERT(set.back() == 1);
  CPPUNIT_ASSERT(to_vector(set) == (std::vector<int>{ 0, 1 }));

  int second = set.pop_back();
  int first  = set.pop_back();

  CPPUNIT_ASSERT(second == 1);
  CPPUNIT_ASSERT(first == 0);
  CPPUNIT_ASSERT(set.empty());

  set.insert_back(3);

  CPPUNIT_ASSERT(set.front() == 3);
  CPPUNIT_ASSERT(set.back() == 3);
}

void
test_linked_unordered_set::test_find() {
  int_set set;

  set.insert_back(1);
  set.insert_back(2);

  CPPUNIT_ASSERT(set.find(1) == set.begin());
  CPPUNIT_ASSERT(set.find(2) == std::next(set.begin()));
  CPPUNIT_ASSERT(set.find(3) == set.end());

  set.erase(2);

  CPPUNIT_ASSERT(set.find(2) == set.end());
}

void
test_linked_unordered_set::test_find_transparent() {
  entry_set set;
  auto      first  = std::make_shared<entry>(entry{ 1 });
  auto      second = std::make_shared<entry>(entry{ 2 });

  set.insert_back(first);
  set.insert_back(second);

  CPPUNIT_ASSERT(*set.find(1) == first);
  CPPUNIT_ASSERT(set.find(2)->get() == second.get());
  CPPUNIT_ASSERT(set.find(3) == set.end());

  bool inserted = set.insert_back(std::make_shared<entry>(entry{ 2 }));

  CPPUNIT_ASSERT(!inserted);
  CPPUNIT_ASSERT(set.size() == 2);
}
