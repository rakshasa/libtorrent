#include "config.h"

#include <algo/algo.h>

#include "ranges.h"

using namespace algo;

namespace torrent {

void
Ranges::add(uint32_t begin, uint32_t end) {
  if (m_list.empty() ||
      m_list.back().second < begin) {

    m_size += end - begin;
    m_list.push_back(Range(begin, end));

  } else if (m_list.back().second < end) {
    m_size += end - begin - (m_list.back().second - begin);
    m_list.back().second = end;
  }
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
