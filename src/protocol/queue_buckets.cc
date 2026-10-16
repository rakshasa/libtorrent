#include "config.h"

#include "protocol/queue_buckets.h"

#include "torrent/data/block.h"

namespace torrent {

bool
QueueBuckets::empty() const {
  for (int i = 0; i < request_list_constants::bucket_count; i++)
    if (!queue_empty(i))
      return false;

  return true;
}

void
QueueBuckets::pop_front(int idx) {
  queue_at(idx).pop_front();

  instrumentation_update(request_list_constants::instrumentation_removed[idx], 1);
  instrumentation_update(request_list_constants::instrumentation_total[idx], -1);
}

void
QueueBuckets::pop_back(int idx) {
  queue_at(idx).pop_back();

  instrumentation_update(request_list_constants::instrumentation_removed[idx], 1);
  instrumentation_update(request_list_constants::instrumentation_total[idx], -1);
}

typename QueueBuckets::value_type
QueueBuckets::pop_and_front(int idx) {
  value_type v = queue_at(idx).front();
  pop_front(idx);
  return v;
}

typename QueueBuckets::value_type
QueueBuckets::pop_and_back(int idx) {
  value_type v = queue_at(idx).back();
  pop_back(idx);
  return v;
}

void
QueueBuckets::push_front(int idx, const value_type& value) {
  queue_at(idx).push_front(value);

  instrumentation_update(request_list_constants::instrumentation_added[idx], 1);
  instrumentation_update(request_list_constants::instrumentation_total[idx], 1);
}

void
QueueBuckets::push_back(int idx, const value_type& value) {
  queue_at(idx).push_back(value);

  instrumentation_update(request_list_constants::instrumentation_added[idx], 1);
  instrumentation_update(request_list_constants::instrumentation_total[idx], 1);
}

typename QueueBuckets::value_type
QueueBuckets::take(int idx, iterator itr) {
  value_type v = *itr;
  queue_at(idx).erase(itr);

  instrumentation_update(request_list_constants::instrumentation_removed[idx], 1);
  instrumentation_update(request_list_constants::instrumentation_total[idx], -1);

  // TODO: Add 'taken' instrumentation.

  return v;
}

void
QueueBuckets::clear(int idx) {
  destroy(idx, begin(idx), end(idx));
}

void
QueueBuckets::destroy(int idx, iterator begin, iterator end) {
  difference_type difference = std::distance(begin, end);
  instrumentation_update(request_list_constants::instrumentation_removed[idx], difference);
  instrumentation_update(request_list_constants::instrumentation_total[idx], -difference);

  // Consider moving these to a temporary dequeue before releasing:
  std::for_each(begin, end, [](BlockTransfer* obj) { Block::release(obj); });

  queue_at(idx).erase(begin, end);
}

void
QueueBuckets::move_to(int src_idx, iterator src_begin, iterator src_end,
                                        int dst_idx) {
  difference_type difference = std::distance(src_begin, src_end);
  instrumentation_update(request_list_constants::instrumentation_moved[src_idx], difference);
  instrumentation_update(request_list_constants::instrumentation_total[src_idx], -difference);
  instrumentation_update(request_list_constants::instrumentation_added[dst_idx], difference);
  instrumentation_update(request_list_constants::instrumentation_total[dst_idx], difference);

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

void
QueueBuckets::move_all_to(int src_idx, int dst_idx) {
  move_to(src_idx, queue_at(src_idx).begin(), queue_at(src_idx).end(), dst_idx);
}

} // namespace torrent
