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

#include "config.h"

#include <algorithm>
#include <functional>

#include "peer_connection.h"
#include "utils/unordered_vector.h"

#include "choke_manager.h"

namespace torrent {

struct ChokeManagerReadRate {
  // TODO: Add graze period here.
  bool operator () (PeerConnectionBase* p1, PeerConnectionBase* p2) const {
    return p1->get_read_rate().rate() >= p2->get_read_rate().rate();
  }
};

inline ChokeManager::iterator
ChokeManager::seperate_interested(iterator first, iterator last) {
  return std::partition(first, last, std::mem_fun(&PeerConnectionBase::is_write_interested));
}

inline ChokeManager::iterator
ChokeManager::seperate_unchoked(iterator first, iterator last) {
  return std::partition(first, last, std::not1(std::mem_fun(&PeerConnectionBase::is_write_choked)));
}

inline void
ChokeManager::sort_read_rate(iterator first, iterator last) {
  std::sort(first, last, ChokeManagerReadRate());
}

int
ChokeManager::get_unchoked(iterator first, iterator last) const {
  return std::distance(first, seperate_unchoked(first, last));
}

void
ChokeManager::balance(iterator first, iterator last) {
  iterator beginUninterested = seperate_interested(first, last);
  iterator beginChoked       = seperate_unchoked(first, beginUninterested);

  int unchoked = std::distance(first, beginChoked);

  if (unchoked < m_maxUnchoked)
    unchoke(beginChoked, beginUninterested, m_maxUnchoked - unchoked);

  else if (unchoked > m_maxUnchoked)
    choke(first, beginChoked, unchoked - m_maxUnchoked);
}

void
ChokeManager::cycle(iterator first, iterator last) {
  // TMP
//   balance(first, last);

  iterator beginUninterested = seperate_interested(first, last);
  iterator beginChoked       = seperate_unchoked(first, beginUninterested);

  int unchoked = std::distance(first, beginChoked);

  
}

void
ChokeManager::choke(iterator first, iterator last, int count) {
  count = std::min(count, std::distance(first, last));

  sort_read_rate(first, last);

  std::for_each(last - count, last,
		std::bind2nd(std::mem_fun(&PeerConnection::choke), true));
}

void
ChokeManager::unchoke(iterator first, iterator last, int count) {
  count = std::min(count, std::distance(first, last));

  sort_read_rate(first, last);

  std::for_each(first, first + count,
		std::bind2nd(std::mem_fun(&PeerConnection::choke), false));
}

}
