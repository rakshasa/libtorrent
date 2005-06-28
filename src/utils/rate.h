// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_UTILS_RATE_H
#define LIBTORRENT_UTILS_RATE_H

#include "utils/timer.h"
#include <deque>

namespace torrent {

// Keep the current rate count up to date for each call to rate() and
// insert(...). This requires a mutable since rate() is const, but if
// justified as we don't need to iterate the deque for each call.

class Rate {
public:
  // std::pair<seconds, bytes>
  typedef std::pair<int32_t, uint32_t> value_type;
  typedef std::deque<value_type>       Container;

  Rate(int32_t span = 30) : m_current(0), m_total(0), m_span(span) {}

  uint32_t            rate() const;
  uint64_t            total() const                         { return m_total; }

  void                insert(uint32_t bytes);
  
  bool                operator < (Rate& r)                  { return rate() < r.rate(); }
  bool                operator > (Rate& r)                  { return rate() > r.rate(); }

private:
  inline void         discard_old() const;

  mutable Container   m_container;
  mutable uint32_t    m_current;
  uint64_t            m_total;

  int32_t             m_span;
};

}

#endif // LIBTORRENT_RATE_H
