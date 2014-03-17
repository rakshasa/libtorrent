// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_QUEUE_BUCKETS_H
#define LIBTORRENT_QUEUE_BUCKETS_H

#include <deque>
#include <tr1/functional>
#include <tr1/array>

// Temporary:
#include "utils/instrumentation.h"

// Temporary:
struct test_constants {
  static const int bucket_count = 2;

  static const torrent::instrumentation_enum instrumentation_added[bucket_count];
  static const torrent::instrumentation_enum instrumentation_removed[bucket_count];
  static const torrent::instrumentation_enum instrumentation_total[bucket_count];

  template <typename Type>
  static void destroy(Type& obj);
};

namespace torrent {

template <typename Type, typename Constants>
class queue_buckets : private std::tr1::array<std::deque<Type>, Constants::bucket_count> {
public:
  typedef std::deque<Type>                                      bucket_type;
  typedef std::tr1::array<bucket_type, Constants::bucket_count> base_type;

  typedef Constants constants;

  typedef typename bucket_type::value_type value_type;
  typedef typename bucket_type::size_type size_type;
  typedef typename bucket_type::difference_type difference_type;

  typedef typename bucket_type::iterator iterator;
  typedef typename bucket_type::const_iterator const_iterator;

  size_type bucket_size(int idx) const { return bucket_at(idx).size(); }

  iterator begin(int idx) { return bucket_at(idx).begin(); }
  iterator end(int idx)   { return bucket_at(idx).end(); }
  const_iterator begin(int idx) const { return bucket_at(idx).begin(); }
  const_iterator end(int idx) const   { return bucket_at(idx).end(); }

  void pop_front(int idx);
  void pop_back(int idx);

  void push_front(int idx, const value_type& value_type);
  void push_back(int idx, const value_type& value_type);

  void erase(int idx, iterator itr);
  void destroy(int idx, iterator begin, iterator end);

  void move_to(int src_idx, iterator src_begin, iterator src_end, int dst_idx);

private:
  bucket_type&       bucket_at(int idx)       { return base_type::operator[](idx); }
  const bucket_type& bucket_at(int idx) const { return base_type::operator[](idx); }
};

//
// Implementation:
//

// TODO: when adding removal/etc of element or ranges do logging on if
// it hit the first element or had to search.

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::pop_front(int idx) {
  bucket_at(idx).pop_front();

  instrumentation_update(constants::instrumentation_removed[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], -1);
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::pop_back(int idx) {
  bucket_at(idx).pop_back();

  instrumentation_update(constants::instrumentation_removed[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], -1);
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::push_front(int idx, const value_type& value) {
  bucket_at(idx).push_front(value);

  instrumentation_update(constants::instrumentation_added[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], 1);
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::push_back(int idx, const value_type& value) {
  bucket_at(idx).push_back(value);

  instrumentation_update(constants::instrumentation_added[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], 1);
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::erase(int idx, iterator itr) {
  bucket_at(idx).erase(itr);

  instrumentation_update(constants::instrumentation_removed[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], -1);
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::destroy(int idx, iterator begin, iterator end) {
  difference_type difference = std::distance(begin, end);
  instrumentation_update(constants::instrumentation_removed[idx], difference);
  instrumentation_update(constants::instrumentation_total[idx], -difference);

  // Consider moving these to a temporary dequeue before releasing:
  std::for_each(begin, end, std::tr1::function<void (value_type)>(&constants::template destroy<value_type>));
  bucket_at(idx).erase(begin, end);
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::move_to(int src_idx, iterator src_begin, iterator src_end,
                                        int dst_idx) {
  difference_type difference = std::distance(src_begin, src_end);
  instrumentation_update(constants::instrumentation_removed[src_idx], difference);
  instrumentation_update(constants::instrumentation_total[src_idx], -difference);
  instrumentation_update(constants::instrumentation_added[dst_idx], difference);
  instrumentation_update(constants::instrumentation_total[dst_idx], difference);

  // TODO: Check for better move operations:
  if (bucket_at(dst_idx).empty() &&
      src_begin == bucket_at(src_idx).begin() &&
      src_end == bucket_at(src_idx).end()) {
    bucket_at(dst_idx).swap(bucket_at(src_idx));
  } else {
    bucket_at(dst_idx).insert(bucket_at(dst_idx).end(), src_begin, src_end);
    bucket_at(src_idx).erase(src_begin, src_end);
  }
}

}

#endif // LIBTORRENT_QUEUE_BUCKETS_H
