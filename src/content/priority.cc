#include "config.h"

#include <algo/algo.h>

#include "torrent/exceptions.h"
#include "priority.h"

using namespace algo;

namespace torrent {

void
Priority::add(Type t, uint32_t begin, uint32_t end) {
  if (m_list[t].empty() ||
      m_list[t].back().second < begin) {

    m_size[t] += end - begin;
    m_list[t].push_back(Range(begin, end));

  } else if (m_list[t].back().second < end) {
    m_size[t] += end - begin - (m_list[t].back().second - begin);
    m_list[t].back().second = end;
  }
}

void
Priority::clear() {
  for (int i = 0; i < 3; ++i) {
    m_size[i] = 0;
    m_list[i].clear();
  }
}

Priority::List::iterator
Priority::find(Type t, uint32_t index) {
  return std::find_if(m_list[t].begin(), m_list[t].end(),
		      lt(value(index), member(&Range::second)));
}

bool
Priority::has(Type t, uint32_t index) {
  List::const_iterator itr = std::find_if(m_list[t].begin(), m_list[t].end(),
					  lt(value(index), member(&Range::second)));

  return itr != m_list[t].end() && index >= itr->first;
}

}
