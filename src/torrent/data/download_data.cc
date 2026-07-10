#include "config.h"

#include "torrent/exceptions.h"

#include "download_data.h"

namespace torrent {

// Calculate the number of chunks remaining to be downloaded.
//
// Doing it the slow and safe way, optimize this at some point.
uint32_t
download_data::calc_wanted_chunks() const {
  if (m_completed_bitfield.is_all_set())
    return 0;

  priority_ranges wanted_ranges = priority_ranges::create_union(m_normal_priority, m_high_priority);

  if (m_completed_bitfield.is_all_unset())
    return wanted_ranges.intersect_distance(0, m_completed_bitfield.size_bits());

  if (m_completed_bitfield.is_all_set())
    return 0;

  if (m_completed_bitfield.empty())
    throw internal_error("download_data::update_wanted_chunks() m_completed_bitfield.empty().");

  uint32_t result = 0;

  for (const auto& wanted_range : wanted_ranges) {
    //remaining = completed->count_range(itr->first, itr->second);

    uint32_t idx = wanted_range.first;

    while (idx != wanted_range.second)
      result += !m_completed_bitfield.get(idx++);
  }

  return result;
}

void
download_data::verify_wanted_chunks(const char* where) const {
  if (m_wanted_chunks != calc_wanted_chunks())
    throw internal_error("Invalid download_data::wanted_chunks() value in " + std::string(where) + ".");
}

} // namespace torrent
