#include "config.h"

#include <functional>
#include <algo/algo.h>

#include "ranges.h"

using namespace algo;

namespace torrent {

void
Ranges::insert(value_type r) {
  // A = Find the last iterator with "first" <= begin. Points one after.
  iterator a = std::find_if(Base::rbegin(), Base::rend(),
			    leq(member(&value_type::first), value(r.first))).base();
			    

  // B = Find the first iterator with "last" >= end.
  iterator b = std::find_if(Base::begin(), Base::end(),
			    geq(member(&value_type::second), value(r.second)));

  // Check if the range is contained inside a single preexisting range.
  if (a != Base::rend().base() && b != Base::end() && --iterator(a) == b)
    return;

  // Erase elements in between begin and end, then update iterator a and b.
  b = Base::erase(a, b);

  // Check if the range spans all the previous ranges.
  if (Base::empty()) {
    Base::push_back(value_type(r.first, r.second));
    return;
  }

  // Add dummy elements if we're at the end of either side.
  if (b == Base::end())
    b = Base::insert(Base::end(), value_type(r.first, r.second));

  else if (b == Base::begin())
    b = ++Base::insert(Base::begin(), value_type(r.first, r.second));

  a = --iterator(b);

  // Does not touch the a and b ranges, create a new.
  if (r.first > a->second && r.second < b->first) {
    Base::insert(b, value_type(r.first, r.second));
    return;

  } else if (r.first <= a->second) {
    a->second = r.second;

  } else if (r.second >= b->first) {
    b->first = r.first;
  }
  
  // If there's no gap, remove one of the elements.
  if (a->second >= b->first) {
    a->second = b->second;
    Base::erase(b);
  }
}

void
Ranges::erase(value_type r) {
  // A = Find the last iterator with "first" <= begin. Points one after.
  iterator a = std::find_if(Base::rbegin(), Base::rend(),
			    lt(member(&value_type::first), value(r.first))).base();

  // B = Find the first iterator with "last" >= end.
  iterator b = std::find_if(Base::begin(), Base::end(),
			    gt(member(&value_type::second), value(r.second)));

  // Erase elements in between begin and end, then update iterator a and b.
  b = Base::erase(a, b);

  if (b != Base::end() && b->first < r.second)
    b->first = r.second;

  if (b != Base::begin() && (--b)->second > r.first)
    b->second = r.first;
}

Ranges::Base::iterator
Ranges::find(uint32_t index) {
  return std::find_if(Base::begin(), Base::end(),
		      lt(value(index), member(&value_type::second)));
}

bool
Ranges::has(uint32_t index) {
  Base::const_iterator itr = std::find_if(Base::begin(), Base::end(),
					  lt(value(index), member(&value_type::second)));

  return itr != Base::end() && index >= itr->first;
}

Ranges&
Ranges::intersect(Ranges& r) {
  std::for_each(r.begin(), r.end(), std::bind1st(std::mem_fun(&Ranges::erase), this));

  return *this;
}

}
