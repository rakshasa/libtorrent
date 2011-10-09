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

  if (m_completed_bitfield.empty())
    throw internal_error("download_data::update_wanted_chunks() m_completed_bitfield.empty().");

  uint32_t result = 0;

  for (download_data::priority_ranges::const_iterator itr = wanted_ranges.begin(), last = wanted_ranges.end(); itr != last; itr++) {
    //remaining = completed->count_range(itr->first, itr->second);

    uint32_t idx = itr->first;

    while (idx != itr->second)
      result += !m_completed_bitfield.get(idx++);
  }

  return result;
}

void
download_data::verify_wanted_chunks(const char* where) const {
  if (m_wanted_chunks != calc_wanted_chunks())
    throw internal_error("Invalid download_data::wanted_chunks() value in " + std::string(where) + ".");
}

}
