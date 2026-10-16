<<<<<<< Updated upstream
#ifndef LIBTORRENT_PROTOCOL_QUEUE_BUCKETS_H
#define LIBTORRENT_PROTOCOL_QUEUE_BUCKETS_H

#include "torrent/utils/linked_unordered_set.h"
#include "torrent/data/block_transfer.h"
#include "utils/instrumentation.h"

namespace torrent {

struct request_list_constants {
  static constexpr int bucket_count = 4;

  static const torrent::instrumentation_enum instrumentation_added[bucket_count];
  static const torrent::instrumentation_enum instrumentation_moved[bucket_count];
  static const torrent::instrumentation_enum instrumentation_removed[bucket_count];
  static const torrent::instrumentation_enum instrumentation_total[bucket_count];
};

class QueueBuckets : private std::array<std::deque<BlockTransfer*>, request_list_constants::bucket_count> {
public:
  using queue_type = std::deque<BlockTransfer*>;
  using base_type  = std::array<queue_type, request_list_constants::bucket_count>;
=======
#ifndef LIBTORRENT_QUEUE_BUCKETS_H
#define LIBTORRENT_QUEUE_BUCKETS_H

// #include <algorithm>
// #include <array>
// #include <deque>
// #include <functional>

#include "torrent/utils/linked_unordered_set.h"
#include "torrent/data/block_transfer.h"

namespace torrent {

template <typename Constants>
class queue_buckets : private std::array<std::deque<BlockTransfer*>, Constants::bucket_count> {
public:
  using queue_type = std::deque<BlockTransfer*>;
  using base_type  = std::array<queue_type, Constants::bucket_count>;

  using constants = Constants;
>>>>>>> Stashed changes

  using value_type      = typename queue_type::value_type;
  using size_type       = typename queue_type::size_type;
  using difference_type = typename queue_type::difference_type;

  using iterator       = typename queue_type::iterator;
  using const_iterator = typename queue_type::const_iterator;

  size_type           queue_size(int idx)  const { return queue_at(idx).size(); }
  bool                queue_empty(int idx) const { return queue_at(idx).empty(); }

  bool                empty() const;

  iterator            begin(int idx)       { return queue_at(idx).begin(); }
  iterator            end(int idx)         { return queue_at(idx).end(); }
  const_iterator      begin(int idx) const { return queue_at(idx).begin(); }
  const_iterator      end(int idx)   const { return queue_at(idx).end(); }

  value_type          front(int idx)       { return queue_at(idx).front(); }
  value_type          back(int idx)        { return queue_at(idx).back(); }
  value_type          front(int idx) const { return queue_at(idx).front(); }
  value_type          back(int idx)  const { return queue_at(idx).back(); }

  void                pop_front(int idx);
  void                pop_back(int idx);

  value_type          pop_and_front(int idx);
  value_type          pop_and_back(int idx);

  void                push_front(int idx, const value_type& value_type);
  void                push_back(int idx, const value_type& value_type);

  value_type          take(int idx, iterator itr);

  void                clear(int idx);
  void                destroy(int idx, iterator begin, iterator end);

  void                move_to(int src_idx, iterator src_begin, iterator src_end, int dst_idx);
  void                move_all_to(int src_idx, int dst_idx);

private:
  queue_type&         queue_at(int idx)       { return base_type::operator[](idx); }
  const queue_type&   queue_at(int idx) const { return base_type::operator[](idx); }
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
<<<<<<< Updated upstream
  for (int i = 0; i < request_list_constants::bucket_count; i++) {
=======
  for (int i = 0; i < QueueBucket::constants::bucket_count; i++) {
>>>>>>> Stashed changes
    auto itr = std::find_if(queues.begin(i), queues.end(i), ftor);

    if (itr != queues.end(i))
      return std::make_pair(i, itr);
  }

<<<<<<< Updated upstream
  return std::make_pair(request_list_constants::bucket_count,
                        queues.end(request_list_constants::bucket_count - 1));
=======
  return std::make_pair(QueueBucket::constants::bucket_count,
                        queues.end(QueueBucket::constants::bucket_count - 1));
>>>>>>> Stashed changes
}

template<typename QueueBucket, typename Ftor>
inline std::pair<int, typename QueueBucket::const_iterator>
queue_bucket_find_if_in_any(const QueueBucket& queues, Ftor ftor) {
<<<<<<< Updated upstream
  for (int i = 0; i < request_list_constants::bucket_count; i++) {
=======
  for (int i = 0; i < QueueBucket::constants::bucket_count; i++) {
>>>>>>> Stashed changes
    typename QueueBucket::const_iterator itr = std::find_if(queues.begin(i), queues.end(i), ftor);

    if (itr != queues.end(i))
      return std::make_pair(i, itr);
  }

<<<<<<< Updated upstream
  return std::make_pair(request_list_constants::bucket_count,
                        queues.end(request_list_constants::bucket_count - 1));
=======
  return std::make_pair(QueueBucket::constants::bucket_count,
                        queues.end(QueueBucket::constants::bucket_count - 1));
}

//
// Implementation:
//

// TODO: Consider renaming bucket to queue.

// TODO: when adding removal/etc of element or ranges do logging on if
// it hit the first element or had to search.

template <typename Constants>
inline bool
queue_buckets<Constants>::empty() const {
  for (int i = 0; i < constants::bucket_count; i++)
    if (!queue_empty(i))
      return false;

  return true;
}

template <typename Constants>
inline void
queue_buckets<Constants>::pop_front(int idx) {
  queue_at(idx).pop_front();

  instrumentation_update(constants::instrumentation_removed[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], -1);
}

template <typename Constants>
inline void
queue_buckets<Constants>::pop_back(int idx) {
  queue_at(idx).pop_back();

  instrumentation_update(constants::instrumentation_removed[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], -1);
}

template <typename Constants>
inline typename queue_buckets<Constants>::value_type
queue_buckets<Constants>::pop_and_front(int idx) {
  value_type v = queue_at(idx).front();
  pop_front(idx);
  return v;
}

template <typename Constants>
inline typename queue_buckets<Constants>::value_type
queue_buckets<Constants>::pop_and_back(int idx) {
  value_type v = queue_at(idx).back();
  pop_back(idx);
  return v;
}

template <typename Constants>
inline void
queue_buckets<Constants>::push_front(int idx, const value_type& value) {
  queue_at(idx).push_front(value);

  instrumentation_update(constants::instrumentation_added[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], 1);
}

template <typename Constants>
inline void
queue_buckets<Constants>::push_back(int idx, const value_type& value) {
  queue_at(idx).push_back(value);

  instrumentation_update(constants::instrumentation_added[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], 1);
}

template <typename Constants>
inline typename queue_buckets<Constants>::value_type
queue_buckets<Constants>::take(int idx, iterator itr) {
  value_type v = *itr;
  queue_at(idx).erase(itr);

  instrumentation_update(constants::instrumentation_removed[idx], 1);
  instrumentation_update(constants::instrumentation_total[idx], -1);

  // TODO: Add 'taken' instrumentation.

  return v;
}

template <typename Constants>
inline void
queue_buckets<Constants>::clear(int idx) {
  destroy(idx, begin(idx), end(idx));
}

template <typename Constants>
inline void
queue_buckets<Constants>::destroy(int idx, iterator begin, iterator end) {
  difference_type difference = std::distance(begin, end);
  instrumentation_update(constants::instrumentation_removed[idx], difference);
  instrumentation_update(constants::instrumentation_total[idx], -difference);

  // Consider moving these to a temporary dequeue before releasing:
  std::for_each(begin, end, std::function<void (value_type&)>(&constants::template destroy<value_type>));
  queue_at(idx).erase(begin, end);
}

template <typename Constants>
inline void
queue_buckets<Constants>::move_to(int src_idx, iterator src_begin, iterator src_end,
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

template <typename Constants>
inline void
queue_buckets<Constants>::move_all_to(int src_idx, int dst_idx) {
  move_to(src_idx, queue_at(src_idx).begin(), queue_at(src_idx).end(), dst_idx);
>>>>>>> Stashed changes
}

} // namespace torrent

<<<<<<< Updated upstream
#endif
=======
#endif // LIBTORRENT_QUEUE_BUCKETS_H
>>>>>>> Stashed changes
