#include "config.h"

#include <algo/algo.h>

#include "torrent/exceptions.h"

#include "tracker_http.h"
#include "tracker_list.h"

namespace torrent {

void
TrackerList::randomize() {
  // Random random random.
  iterator itr = begin();
  
  while (itr != end()) {
    iterator tmp = end_group(itr->first);

    std::random_shuffle(itr, tmp);

    itr = tmp;
  }
}

void
TrackerList::clear() {
  std::for_each(begin(), end(), algo::delete_on(&value_type::second));

  Base::clear();
}

void
TrackerList::promote(iterator itr) {
  iterator beg = begin_group(itr->first);

  if (beg == end())
    throw internal_error("torrent::TrackerList::promote(...) Could not find beginning of group");

  // GCC 3.3 bug, don't use yet.
  //std::swap(beg, itr);

  value_type tmp = *beg;
  *beg = *itr;
  *itr = tmp;
}

TrackerList::iterator
TrackerList::begin_group(int group) {
  return std::find_if(begin(), end(), algo::leq(algo::value(group), algo::member(&value_type::first)));
}

}
