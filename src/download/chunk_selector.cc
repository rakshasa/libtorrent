#include "config.h"

#include "download/chunk_selector.h"

#include <algorithm>
#include <cstdlib>

#include "download/chunk_statistics.h"
#include "protocol/peer_chunks.h"
#include "torrent/exceptions.h"

namespace torrent {

// Consider making statistics a part of selector.
void
ChunkSelector::initialize(ChunkStatistics* cs) {
  m_position = invalid_chunk;
  m_high_selection_balance = 0;
  m_statistics = cs;

  Bitfield* completed = m_data->mutable_completed_bitfield();
  Bitfield* untouched = m_data->mutable_untouched_bitfield();

  untouched->set_size_bits(completed->size_bits());
  untouched->allocate();
  std::transform(completed->begin(), completed->end(), untouched->begin(), [](const Bitfield::value_type& v) { return ~v; });
  untouched->update();

  m_shared_queue.enable(32);
  m_shared_queue.clear();
}

void
ChunkSelector::cleanup() {
  m_data->mutable_untouched_bitfield()->clear();
  m_statistics = NULL;
}

// Consider if ChunksSelector::not_using_index(...) needs to be
// modified.
void
ChunkSelector::update_priorities() {
  m_high_selection_balance = 0;

  if (empty())
    return;

  m_shared_queue.clear();

  if (m_position == invalid_chunk)
    m_position = random() % size();

  advance_position();
}

ChunkSelector::find_result
ChunkSelector::find(PeerChunks* pc) {
  // This needs to be re-enabled.
  if (m_position == invalid_chunk)
    return {invalid_chunk, false};

  // When we're a seeder, 'm_sharedQueue' is used. Since the peer's
  // bitfield is guaranteed to be filled we can use the same code as
  // for non-seeders. This generalization does incur a slight
  // performance hit as it compares against a bitfield we know has all
  // set.
  auto* queue = pc->is_seeder() ? &m_shared_queue : pc->download_cache();

  // Randomize position on average every 16 chunks to prevent
  // inefficient distribution with a slow seed and fast peers
  // all arriving at the same position.
  if ((random() & 63) == 0) {
    m_position = random() % size();
    queue->clear();
  }

  if (queue->is_enabled()) {

    // First check the cached queue.
    while (queue->prepare_pop()) {
      auto entry = queue->pop();

      if (!m_data->untouched_bitfield()->get(entry.index))
        continue;

      m_high_selection_balance += entry.high_priority ? 1 : -3;
      return {entry.index, entry.high_priority};
    }

  } else {
    // Only non-seeders can reach this branch as m_shared_queue is
    // always enabled.
    queue->enable(8);
  }

  queue->clear();

  auto search_priority = [&](bool high) {
    const auto* ranges = high ? m_data->high_priority() : m_data->normal_priority();

    if (search_linear(pc->bitfield(), queue, ranges, high, m_position, size()))
      search_linear(pc->bitfield(), queue, ranges, high, 0, m_position);
  };

  bool try_high = m_high_selection_balance < 3;

  search_priority(try_high);

  if (!queue->prepare_pop()) {
    queue->clear();

    try_high = !try_high;
    search_priority(try_high);

    if (!queue->prepare_pop())
      return {invalid_chunk, false};
  }

  auto entry = queue->pop();

  if (!m_data->untouched_bitfield()->get(entry.index))
    throw internal_error("ChunkSelector::find(...) bad index.");

  m_high_selection_balance += entry.high_priority ? 1 : -3;

  return {entry.index, entry.high_priority};
}

void
ChunkSelector::using_index(uint32_t index) {
  if (index >= size())
    throw internal_error("ChunkSelector::select_index(...) index out of range.");

  if (!m_data->untouched_bitfield()->get(index))
    throw internal_error("ChunkSelector::select_index(...) index already set.");

  m_data->mutable_untouched_bitfield()->unset(index);

  // We always know 'm_position' points to a wanted chunk. If it
  // changes, we need to move m_position to the next one.
  if (index == m_position)
    advance_position();
}

void
ChunkSelector::not_using_index(uint32_t index) {
  if (index >= size())
    throw internal_error("ChunkSelector::deselect_index(...) index out of range.");

  if (m_data->untouched_bitfield()->get(index))
    throw internal_error("ChunkSelector::deselect_index(...) index already unset.");

  m_data->mutable_untouched_bitfield()->set(index);

  // This will make sure that if we enable new chunks, it will start
  // downloading them event when 'index == invalid_chunk'.
  if (m_position == invalid_chunk)
    m_position = index;
}

// This could propably be split into two functions, one for checking
// if it shoul insert into the request_list(), and the other
// whetever we are interested in the new piece.
//
// Since this gets called whenever a new piece arrives, we can skip
// the rarity-first/linear etc searches through bitfields and just
// start downloading.
bool
ChunkSelector::received_have_chunk(PeerChunks* pc, uint32_t index) {
  if (!m_data->untouched_bitfield()->get(index))
    return false;

  bool has_high = m_data->high_priority()->has(index);

  // Also check if the peer only has high-priority chunks.
  if (!has_high && !m_data->normal_priority()->has(index))
    return false;

  if (pc->download_cache()->is_enabled())
    pc->download_cache()->insert(m_statistics->rarity(index), index, has_high);

  return true;
}

bool
ChunkSelector::search_linear(const Bitfield* bf, utils::PartialQueue* pq, const download_data::priority_ranges* ranges, bool high_priority, uint32_t first, uint32_t last) {
  auto itr = ranges->find(first);

  while (itr != ranges->end() && itr->first < last) {

    if (!search_linear_range(bf, pq, high_priority, std::max(first, itr->first), std::min(last, itr->second)))
      return false;

    ++itr;
  }

  return true;
}

// Could propably add another argument for max seen or something, this
// would be used to find better chunks to request.
inline bool
ChunkSelector::search_linear_range(const Bitfield* bf, utils::PartialQueue* pq, bool high_priority, uint32_t first, uint32_t last) {
  if (first >= last || last > size())
    throw internal_error("ChunkSelector::search_linear_range(...) received an invalid range.");

  auto local  = m_data->untouched_bitfield()->begin() + first / 8;
  auto source = bf->begin() + first / 8;

  // Unset any bits before 'first'.
  Bitfield::value_type wanted = (*source & *local) & Bitfield::mask_from(first % 8);

  while (m_data->untouched_bitfield()->position(local + 1) < last) {
    if (wanted && !search_linear_byte(pq, high_priority, m_data->untouched_bitfield()->position(local), wanted))
      return false;

    wanted = (*++source & *++local);
  }

  // Unset any bits from 'last'.
  wanted &= Bitfield::mask_before(last - m_data->untouched_bitfield()->position(local));

  if (wanted)
    return search_linear_byte(pq, high_priority, m_data->untouched_bitfield()->position(local), wanted);
  else
    return true;
}

// Take pointer to partial_queue
inline bool
ChunkSelector::search_linear_byte(utils::PartialQueue* pq, bool high_priority, uint32_t index, Bitfield::value_type wanted) {
  for (int i = 0; i < 8; ++i) {
    if (!(wanted & Bitfield::mask_at(i)))
      continue;

    if (!pq->insert(m_statistics->rarity(index + i), index + i, high_priority) && pq->is_full())
      return false;
  }

  return true;
}

// First untouched index in [first, last) that lies in priority ranges.
static uint32_t
next_wanted_in_ranges(const Bitfield* untouched,
                      const download_data::priority_ranges* ranges,
                      uint32_t first,
                      uint32_t last) {
  if (first >= last)
    return ChunkSelector::invalid_chunk;

  for (auto itr = ranges->find(first); itr != ranges->end() && itr->first < last; ++itr) {
    uint32_t range_first = std::max(first, itr->first);
    uint32_t range_last  = std::min(last, itr->second);

    uint32_t chunk = untouched->find_first_set(range_first, range_last);

    if (chunk != range_last)
      return chunk;
  }

  return ChunkSelector::invalid_chunk;
}

void
ChunkSelector::advance_position() {
  if (empty()) {
    m_position = invalid_chunk;
    return;
  }

  // Start from the current cursor (inclusive). Callers either leave a random /
  // sticky index that may still be wanted, or have just cleared m_position via
  // using_index so this index is no longer untouched and we move past it.
  uint32_t start = m_position;

  if (start == invalid_chunk || start >= size())
    start = 0;

  const Bitfield* untouched = m_data->untouched_bitfield();
  const auto*     high      = m_data->high_priority();
  const auto*     normal    = m_data->normal_priority();

  // Prefer high priority, then normal; wrap [start, size) then [0, start).
  uint32_t pos = next_wanted_in_ranges(untouched, high, start, size());

  if (pos == invalid_chunk)
    pos = next_wanted_in_ranges(untouched, high, 0, start);
  if (pos == invalid_chunk)
    pos = next_wanted_in_ranges(untouched, normal, start, size());
  if (pos == invalid_chunk)
    pos = next_wanted_in_ranges(untouched, normal, 0, start);

  m_position = pos;
}

} // namespace torrent
