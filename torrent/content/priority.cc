#include "config.h"

#include "exceptions.h"
#include "priority.h"

namespace torrent {

void
Priority::clear() {
  for (int i = 0; i < 3; ++i) {
    m_size[i] = 0;
    m_list[i].clear();
  }
}

Priority::Position             
Priority::find(Type t, unsigned int index) {
  for (List::const_iterator itr = m_list[t].begin();
       itr != m_list[t].end();
       index -= itr->second - itr->first, ++itr)
    
    if (index < itr->second - itr->first)
      return Position(index, *itr);

  throw internal_error("Priority::find_relative(...) called on an empty priority or with too large index");
}

}
