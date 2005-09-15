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

#include <algorithm>
#include <functional>

#include "protocol/peer_connection_base.h"
#include "utils/unordered_vector.h"

#include "choke_manager.h"

namespace torrent {

struct ChokeManagerReadRate {
  bool operator () (PeerConnectionBase* p1, PeerConnectionBase* p2) const {
    return p1->get_down_rate().rate() > p2->get_down_rate().rate();
  }
};

struct choke_manager_is_interested {
  bool operator () (PeerConnectionBase* p) const {
    return p->is_down_interested() && !p->is_snubbed();
  }
};

inline ChokeManager::iterator
ChokeManager::seperate_interested(iterator first, iterator last) {
  return std::partition(first, last, choke_manager_is_interested());
}

inline ChokeManager::iterator
ChokeManager::seperate_unchoked(iterator first, iterator last) {
  return std::partition(first, last, std::not1(std::mem_fun(&PeerConnectionBase::is_up_choked)));
}

inline void
ChokeManager::sort_down_rate(iterator first, iterator last) {
  std::sort(first, last, ChokeManagerReadRate());
}

// int
// ChokeManager::get_unchoked(iterator first, iterator last) const {
//   return std::distance(first, seperate_unchoked(first, last));
// }

void
ChokeManager::balance() {
  iterator beginUninterested = seperate_interested(m_connectionList->begin(), m_connectionList->end());
  iterator beginChoked       = seperate_unchoked(m_connectionList->begin(), beginUninterested);

  int unchoked = std::distance(m_connectionList->begin(), beginChoked);

  if (unchoked < m_maxUnchoked)
    unchoke_range(beginChoked, beginUninterested, m_maxUnchoked - unchoked);

  else if (unchoked > m_maxUnchoked)
    choke_range(m_connectionList->begin(), beginChoked, unchoked - m_maxUnchoked);
}

void
ChokeManager::cycle() {
  iterator beginUninterested = seperate_interested(m_connectionList->begin(), m_connectionList->end());
  iterator beginChoked       = seperate_unchoked(m_connectionList->begin(), beginUninterested);

  // Partition away the connections we shall not choke.

  int size = std::min<int>(std::min(std::distance(m_connectionList->begin(), beginChoked),
				    std::distance(beginChoked, beginUninterested)),
			   m_cycleSize);

  if (size == 0)
    return;

  choke_range(m_connectionList->begin(), beginChoked, size);
  unchoke_range(beginChoked, beginUninterested, size);
}

void
ChokeManager::choke(PeerConnectionBase* pc) {
  if (!pc->is_up_choked())
    return;

  pc->set_choke(true);

  balance();
}

void
ChokeManager::try_unchoke(PeerConnectionBase* pc) {
  if (!pc->is_up_choked())
    return;

  int unchoked = std::distance(m_connectionList->begin(), seperate_unchoked(m_connectionList->begin(), m_connectionList->end()));
  
  if (unchoked >= m_maxUnchoked)
    return;

  pc->set_choke(false);
}

// We might no longer be in m_connectionList.
void
ChokeManager::disconnected(PeerConnectionBase* pc) {
  if (pc->is_up_choked())
    return;

  balance();
}

void
ChokeManager::choke_range(iterator first, iterator last, int count) {
  count = std::min<int>(count, std::distance(first, last));

  if (count < 0)
    throw internal_error("ChokeManager::choke(...) got count < 0");

  sort_down_rate(first, last);

  std::for_each(last - count, last,
		std::bind2nd(std::mem_fun(&PeerConnectionBase::set_choke), true));
}

void
ChokeManager::unchoke_range(iterator first, iterator last, int count) {
  count = std::min<int>(count, std::distance(first, last));

  if (count < 0)
    throw internal_error("ChokeManager::unchoke(...) got count < 0");

  sort_down_rate(first, last);

  std::for_each(first, first + count,
		std::bind2nd(std::mem_fun(&PeerConnectionBase::set_choke), false));
}

}
