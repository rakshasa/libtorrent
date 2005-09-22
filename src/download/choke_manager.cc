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

#include "choke_manager.h"

namespace torrent {

ChokeManager::~ChokeManager() {
  if (m_currentlyUnchoked != 0)
    throw internal_error("ChokeManager::~ChokeManager() called but m_currentlyUnchoked != 0.");

  if (m_currentlyUnchoked != 0)
    throw internal_error("ChokeManager::~ChokeManager() called but m_currentlyInterested != 0.");
}

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

// Would propably replace maxUnchoked with whatever max the global
// manager gives us. When trying unchoke, we always ask the global
// manager for permission to unchoke, and when we did a choke.
//
// Don't notify the global manager anything when we keep the balance.

void
ChokeManager::balance() {
  // Return if no balance is needed.

  iterator beginUninterested = seperate_interested(m_connectionList->begin(), m_connectionList->end());
  iterator beginChoked       = seperate_unchoked(m_connectionList->begin(), beginUninterested);

  int adjust = std::min(m_maxUnchoked, m_quota) - m_currentlyUnchoked;

  if (adjust > 0) {
    // Can unchoke peers.

    // Sort.

    while (beginUninterested != beginChoked &&
	   adjust-- &&
	   m_slotUnchoke()) {
      (*--beginUninterested)->receive_choke(false);

      m_currentlyUnchoked++;
    }

  } else {
    unsigned int size = std::min<unsigned int>(-adjust, std::distance(m_connectionList->begin(), beginChoked));

    // Sort.

    // We do the signaling to the ResourceManager before choking so
    // that it won't try to unchoke the same connections.
    for (int i = 0; i < size; ++i)
      m_slotChoke();

    choke_range(m_connectionList->begin(), beginChoked, size);
    m_currentlyUnchoked -= size;
  }
}

int
ChokeManager::cycle(unsigned int quota) {
  iterator beginUninterested = seperate_interested(m_connectionList->begin(), m_connectionList->end());
  iterator beginChoked       = seperate_unchoked(m_connectionList->begin(), beginUninterested);

  // Partition away the connections we shall not choke.

  if (std::distance(m_connectionList->begin(), beginChoked) != m_currentlyUnchoked)
    throw internal_error("ChokeManager::cycle() std::distance(m_connectionList->begin(), beginChoked) != m_currentlyUnchoked.");

  if (std::distance(m_connectionList->begin(), beginUninterested) != m_currentlyInterested)
    throw internal_error("ChokeManager::cycle() std::distance(m_connectionList->begin(), beginChoked) != m_currentlyUnchoked.");

  // Sort ranges.

  iterator beginUnchoked = m_connectionList->begin();

  int cycled = 0;
  int adjust = std::min(quota, m_maxUnchoked) - m_currentlyUnchoked;

  // We don't call the resource manager slots, the number of un/choked
  // connections is returned.

  if (adjust > 0) {
    // Unchoke peers.
    while (beginUninterested != beginChoked &&
	   adjust-- &&
	   m_slotUnchoke()) {

      (*--beginUninterested)->receive_choke(false);
      cycled++;
    }

  } else if (adjust < 0) {
    // Choke peers.
    while (beginUnchoked != beginChoked &&
	   adjust++) {

      m_slotChoke();
      (*beginUnchoked++)->receive_choke(true);
      cycled--;
    }
  }

  // Make sure beginUnchoked starts with the least desirable
  // connections.

  adjust = m_cycleSize - std::abs(cycled);

  // Disable this for the time being.
  if (false &&
      adjust > 0) {
    unsigned int size = std::min(std::min(std::distance(beginUnchoked, beginChoked),
					  std::distance(beginChoked, beginUninterested)),
				 adjust);

    choke_range(beginUnchoked, beginChoked, size);
    unchoke_range(beginChoked, beginUninterested, size);
  }

  // Returned unchoked this cycle.
  return cycled;
}

void
ChokeManager::set_interested(PeerConnectionBase* pc) {
  m_currentlyInterested++;

  if (!pc->is_up_choked())
    return;

  if (m_currentlyUnchoked < m_maxUnchoked &&
      pc->time_last_choked() + 10 * 1000000 < Timer::cache() &&
      m_slotUnchoke()) {
    pc->receive_choke(false);
    m_currentlyUnchoked++;
  }
}

void
ChokeManager::set_not_interested(PeerConnectionBase* pc) {
  m_currentlyInterested--;

  if (pc->is_up_choked())
    return;

  pc->receive_choke(true);

  m_currentlyUnchoked--;
  m_slotChoke();
}

// We might no longer be in m_connectionList.
void
ChokeManager::disconnected(PeerConnectionBase* pc) {
  if (pc->is_down_interested())
    m_currentlyInterested--;

  if (!pc->is_up_choked()) {
    m_currentlyUnchoked--;
    m_slotChoke();
  }
}

void
ChokeManager::choke_range(iterator first, iterator last, unsigned int count) {
  if (count > std::distance(first, last))
    throw internal_error("ChokeManager::choke_range(...) got count > std::distance(first, last).");

  // Use more complex sorting, use nth_element.
  sort_down_rate(first, last);

  std::for_each(last - count, last,
		std::bind2nd(std::mem_fun(&PeerConnectionBase::receive_choke), true));
}
  
void
ChokeManager::unchoke_range(iterator first, iterator last, unsigned int count) {
  if (count > std::distance(first, last))
    throw internal_error("ChokeManager::unchoke_range(...) got count > std::distance(first, last).");

  sort_down_rate(first, last);

  std::for_each(first, first + count,
		std::bind2nd(std::mem_fun(&PeerConnectionBase::receive_choke), false));
}

}
