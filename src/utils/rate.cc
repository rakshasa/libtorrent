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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rate.h"
#include "settings.h"

namespace torrent {

inline void
Rate::discard_old() const {
  while (!m_container.empty() && m_container.back().first < Timer::cache().seconds() - m_span) {
    m_current -= m_container.back().second;
    m_container.pop_back();
  }
}

uint32_t
Rate::rate() const {
  discard_old();

  //  return m_current / std::max<uint32_t>(10, data_time_span());
  return m_current / m_span;
}

void
Rate::insert(uint32_t bytes) {
  discard_old();

  if (m_container.empty() || m_container.front().first != Timer::cache().seconds())
    m_container.push_front(value_type(Timer::cache().seconds(), bytes));
  else
    m_container.front().second += bytes;

  m_total += bytes;
  m_current += bytes;
}
  

}
