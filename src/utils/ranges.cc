#include "config.h"

#include <algo/algo.h>

#include "ranges.h"

using namespace algo;

namespace torrent {

void
Ranges::insert(uint32_t begin, uint32_t end) {
  // A = Find the last iterator with "first" <= begin. Points one after.
  reverse_iterator a = std::find_if(m_list.rbegin(), m_list.rend(),
				    leq(member(&Range::first), value(begin)));

  // B = Find the first iterator with "last" >= end.
  iterator b = std::find_if(m_list.begin(), m_list.end(),
			    geq(member(&Range::second), value(end)));

  // Check if the range is contained inside a single preexisting range.
  if (a != m_list.rend() && b != m_list.end() && --a.base() == b)
    return;

  // Erase elements in between begin and end, then update iterator a and b.
  b = m_list.erase(a.base(), b);

  // Check if the range spans all the previous ranges.
  if (m_list.empty()) {
    m_list.push_back(Range(begin, end));
    return;
  }

  // Add dummy elements if we're at the end of either side.
  if (b == m_list.end())
    b = m_list.insert(m_list.end(), Range(begin, end));

  else if (b == m_list.begin())
    b = ++m_list.insert(m_list.begin(), Range(begin, end));

  iterator a2 = --iterator(b);

  // Does not touch the a and b ranges, create a new.
  if (begin > a2->second && end < b->first) {
    m_list.insert(b, Range(begin, end));
    return;

  } else if (begin <= a2->second) {
    a2->second = end;

  } else if (end >= b->first) {
    b->first = begin;
  }
  
  // If there's no gap, remove one of the elements.
  if (a2->second >= b->first) {
    a2->second = b->second;
    m_list.erase(b);
  }
}

void
Ranges::erase(uint32_t begin, uint32_t end) {
  
}

Ranges::List::iterator
Ranges::find(uint32_t index) {
  return std::find_if(m_list.begin(), m_list.end(),
		      lt(value(index), member(&Range::second)));
}

bool
Ranges::has(uint32_t index) {
  List::const_iterator itr = std::find_if(m_list.begin(), m_list.end(),
					  lt(value(index), member(&Range::second)));

  return itr != m_list.end() && index >= itr->first;
}

}
