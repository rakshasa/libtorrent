#include "config.h"

#include <algo/algo.h>

#include "ranges.h"

using namespace algo;

namespace torrent {

void
Ranges::insert(uint32_t begin, uint32_t end) {
  // A = Find the last iterator with "first" <= begin. Points one after.
  iterator a = std::find_if(List::rbegin(), List::rend(),
			    leq(member(&Range::first), value(begin))).base();

  // B = Find the first iterator with "last" >= end.
  iterator b = std::find_if(List::begin(), List::end(),
			    geq(member(&Range::second), value(end)));

  // Check if the range is contained inside a single preexisting range.
  if (a != List::rend().base() && b != List::end() && --iterator(a) == b)
    return;

  // Erase elements in between begin and end, then update iterator a and b.
  b = List::erase(a, b);

  // Check if the range spans all the previous ranges.
  if (List::empty()) {
    List::push_back(Range(begin, end));
    return;
  }

  // Add dummy elements if we're at the end of either side.
  if (b == List::end())
    b = List::insert(List::end(), Range(begin, end));

  else if (b == List::begin())
    b = ++List::insert(List::begin(), Range(begin, end));

  a = --iterator(b);

  // Does not touch the a and b ranges, create a new.
  if (begin > a->second && end < b->first) {
    List::insert(b, Range(begin, end));
    return;

  } else if (begin <= a->second) {
    a->second = end;

  } else if (end >= b->first) {
    b->first = begin;
  }
  
  // If there's no gap, remove one of the elements.
  if (a->second >= b->first) {
    a->second = b->second;
    List::erase(b);
  }
}

void
Ranges::erase(uint32_t begin, uint32_t end) {
  // A = Find the last iterator with "first" <= begin. Points one after.
  iterator a = std::find_if(List::rbegin(), List::rend(),
			    lt(member(&Range::first), value(begin))).base();

  // B = Find the first iterator with "last" >= end.
  iterator b = std::find_if(List::begin(), List::end(),
			    gt(member(&Range::second), value(end)));

  // Erase elements in between begin and end, then update iterator a and b.
  b = List::erase(a, b);

  if (b != List::end() && b->first < end)
    b->first = end;

  if (b != List::begin() && (--b)->second > begin)
    b->second = begin;
}

Ranges::List::iterator
Ranges::find(uint32_t index) {
  return std::find_if(List::begin(), List::end(),
		      lt(value(index), member(&Range::second)));
}

bool
Ranges::has(uint32_t index) {
  List::const_iterator itr = std::find_if(List::begin(), List::end(),
					  lt(value(index), member(&Range::second)));

  return itr != List::end() && index >= itr->first;
}

}
