#ifndef LIBTORRENT_TRACKER_TRACKER_LIST_H
#define LIBTORRENT_TRACKER_TRACKER_LIST_H

#include <vector>

namespace torrent {

class TrackerHttp;

// The tracker list will contain a list of tracker, divided into
// subgroups. Each group must be randomized before we start. When
// starting the tracker request, always start from the beginning and
// iterate if the request failed. Upon request success move the
// tracker to the beginning of the subgroup and start from the
// beginning of the whole list.

class TrackerList : private std::vector<std::pair<int, TrackerHttp*> > {
public:
  typedef std::vector<std::pair<int, TrackerHttp*> > Base;

  using Base::value_type;

  using Base::iterator;
  using Base::reverse_iterator;
  using Base::size;

  using Base::begin;
  using Base::end;
  using Base::rbegin;
  using Base::rend;

  using Base::operator[];

  ~TrackerList() { clear(); }

  void     randomize();
  void     clear();

  iterator insert(int group, TrackerHttp* t) { return Base::insert(end_group(group), value_type(group, t)); }

  void     promote(iterator itr);

  iterator begin_group(int group);
  iterator end_group(int group)              { return begin_group(group + 1); }
};

}

#endif
