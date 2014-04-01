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

#include <algorithm>
#include <deque>
#include <tr1/functional>
#include <tr1/array>

namespace torrent {

template <typename Type, typename Constants>
class queue_buckets : private std::tr1::array<std::deque<Type>, Constants::bucket_count> {
public:
  typedef std::deque<Type>                                     queue_type;
  typedef std::tr1::array<queue_type, Constants::bucket_count> base_type;

  typedef Constants constants;

  typedef typename queue_type::value_type value_type;
  typedef typename queue_type::size_type size_type;
  typedef typename queue_type::difference_type difference_type;

  typedef typename queue_type::iterator iterator;
  typedef typename queue_type::const_iterator const_iterator;

  size_type queue_size(int idx)  const { return queue_at(idx).size(); }
  bool      queue_empty(int idx) const { return queue_at(idx).empty(); }

  bool             empty() const;

  iterator         begin(int idx)       { return queue_at(idx).begin(); }
  iterator         end(int idx)         { return queue_at(idx).end(); }
  const_iterator   begin(int idx) const { return queue_at(idx).begin(); }
  const_iterator   end(int idx)   const { return queue_at(idx).end(); }

  value_type       front(int idx)       { return queue_at(idx).front(); }
  value_type       back(int idx)        { return queue_at(idx).back(); }
  const value_type front(int idx) const { return queue_at(idx).front(); }
  const value_type back(int idx)  const { return queue_at(idx).back(); }

  void pop_front(int idx);
  void pop_back(int idx);

  value_type pop_and_front(int idx);
  value_type pop_and_back(int idx);

  void push_front(int idx, const value_type& value_type);
  void push_back(int idx, const value_type& value_type);

  value_type take(int idx, iterator itr);

  void clear(int idx);
  void destroy(int idx, iterator begin, iterator end);

  void move_to(int src_idx, iterator src_begin, iterator src_end, int dst_idx);
  void move_all_to(int src_idx, int dst_idx);

private:
  queue_type&       queue_at(int idx)       { return base_type::operator[](idx); }
  const queue_type& queue_at(int idx) const { return base_type::operator[](idx); }
};

//
// Helper methods:
//

template<typename QueueBucket, typename Ftor>
void
queue_bucket_for_all_in_queue(QueueBucket& queues, int idx, Ftor ftor) {
  std::for_each(queues.begin(idx), queues.end(idx), ftor);
}

template<typename QueueBucket, typename Ftor>
void
queue_bucket_for_all_in_queue(const QueueBucket& queues, int idx, Ftor ftor) {
  std::for_each(queues.begin(idx), queues.end(idx), ftor);
}

template<typename QueueBucket, typename Ftor>
inline typename QueueBucket::iterator
queue_bucket_find_if_in_queue(QueueBucket& queues, int idx, Ftor ftor) {
  return std::find_if(queues.begin(idx), queues.end(idx), ftor);
}

template<typename QueueBucket, typename Ftor>
inline typename QueueBucket::const_iterator
queue_bucket_find_if_in_queue(const QueueBucket& queues, int idx, Ftor ftor) {
  return std::find_if(queues.begin(idx), queues.end(idx), ftor);
}

template<typename QueueBucket, typename Ftor>
inline std::pair<int, typename QueueBucket::iterator>
queue_bucket_find_if_in_any(QueueBucket& queues, Ftor ftor) {
  for (int i = 0; i < QueueBucket::constants::bucket_count; i++) {
    typename QueueBucket::iterator itr = std::find_if(queues.begin(i), queues.end(i), ftor);

    if (itr != queues.end(i))
      return std::make_pair(i, itr);
  }

  return std::make_pair(QueueBucket::constants::bucket_count,
                        queues.end(QueueBucket::constants::bucket_count - 1));
}

template<typename QueueBucket, typename Ftor>
inline std::pair<int, typename QueueBucket::const_iterator>
queue_bucket_find_if_in_any(const QueueBucket& queues, Ftor ftor) {
  for (int i = 0; i < QueueBucket::constants::bucket_count; i++) {
    typename QueueBucket::const_iterator itr = std::find_if(queues.begin(i), queues.end(i), ftor);

    if (itr != queues.end(i))
      return std::make_pair(i, itr);
  }

  return std::make_pair(QueueBucket::constants::bucket_count,
                        queues.end(QueueBucket::constants::bucket_count - 1));
}

//
// Implementation:
//

// TODO: Consider renaming bucket to queue.

// TODO: when adding removal/etc of element or ranges do logging on if
// it hit the first element or had to search.

template <typename Type, typename Constants>
inline bool
queue_buckets<Type, Constants>::empty() const {
  for (int i = 0; i < constants::bucket_count; i++)
    if (!queue_empty(i))
      return false;

  return true;
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::pop_front(int idx) {
  queue_at(idx).pop_front();

  instrumentation_update(constants::instrumentation_removed[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], -1);
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::pop_back(int idx) {
  queue_at(idx).pop_back();

  instrumentation_update(constants::instrumentation_removed[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], -1);
}

template <typename Type, typename Constants>
inline typename queue_buckets<Type, Constants>::value_type
queue_buckets<Type, Constants>::pop_and_front(int idx) {
  value_type v = queue_at(idx).front();
  pop_front(idx);
  return v;
}

template <typename Type, typename Constants>
inline typename queue_buckets<Type, Constants>::value_type
queue_buckets<Type, Constants>::pop_and_back(int idx) {
  value_type v = queue_at(idx).back();
  pop_back(idx);
  return v;
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::push_front(int idx, const value_type& value) {
  queue_at(idx).push_front(value);

  instrumentation_update(constants::instrumentation_added[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], 1);
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::push_back(int idx, const value_type& value) {
  queue_at(idx).push_back(value);

  instrumentation_update(constants::instrumentation_added[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], 1);
}

template <typename Type, typename Constants>
inline typename queue_buckets<Type, Constants>::value_type
queue_buckets<Type, Constants>::take(int idx, iterator itr) {
  value_type v = *itr;
  queue_at(idx).erase(itr);

  instrumentation_update(constants::instrumentation_removed[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], -1);

  // TODO: Add 'taken' instrumentation.

  return v;
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::clear(int idx) {
  destroy(idx, begin(idx), end(idx));
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::destroy(int idx, iterator begin, iterator end) {
  difference_type difference = std::distance(begin, end);
  instrumentation_update(constants::instrumentation_removed[idx], difference);
  instrumentation_update(constants::instrumentation_total[idx], -difference);

  // Consider moving these to a temporary dequeue before releasing:
  std::for_each(begin, end, std::tr1::function<void (value_type)>(&constants::template destroy<value_type>));
  queue_at(idx).erase(begin, end);
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::move_to(int src_idx, iterator src_begin, iterator src_end,
                                        int dst_idx) {
  difference_type difference = std::distance(src_begin, src_end);
  instrumentation_update(constants::instrumentation_moved[src_idx], difference);
  instrumentation_update(constants::instrumentation_total[src_idx], -difference);
  instrumentation_update(constants::instrumentation_added[dst_idx], difference);
  instrumentation_update(constants::instrumentation_total[dst_idx], difference);

  // TODO: Check for better move operations:
  if (queue_at(dst_idx).empty() &&
      src_begin == queue_at(src_idx).begin() &&
      src_end == queue_at(src_idx).end()) {
    queue_at(dst_idx).swap(queue_at(src_idx));
  } else {
    queue_at(dst_idx).insert(queue_at(dst_idx).end(), src_begin, src_end);
    queue_at(src_idx).erase(src_begin, src_end);
  }
}

template <typename Type, typename Constants>
inline void
queue_buckets<Type, Constants>::move_all_to(int src_idx, int dst_idx) {
  move_to(src_idx, queue_at(src_idx).begin(), queue_at(src_idx).end(), dst_idx);
}

}

#endif // LIBTORRENT_QUEUE_BUCKETS_H
