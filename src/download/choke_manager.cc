// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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
#include <stdlib.h>

#include "protocol/peer_connection_base.h"

#include "choke_manager.h"

namespace torrent {

ChokeManager::~ChokeManager() {
  if (m_currentlyUnchoked != 0)
    throw internal_error("ChokeManager::~ChokeManager() called but m_currentlyUnchoked != 0.");

  if (m_currentlyUnchoked != 0)
    throw internal_error("ChokeManager::~ChokeManager() called but m_currentlyInterested != 0.");
}

struct choke_manager_read_rate_increasing {
  bool operator () (PeerConnectionBase* p1, PeerConnectionBase* p2) const {
    return *p1->peer_chunks()->download_throttle()->rate() < *p2->peer_chunks()->download_throttle()->rate();
  }
};

struct choke_manager_write_rate_increasing {
  bool operator () (PeerConnectionBase* p1, PeerConnectionBase* p2) const {
    return *p1->peer_chunks()->upload_throttle()->rate() < *p2->peer_chunks()->upload_throttle()->rate();
  }
};

struct choke_manager_read_rate_decreasing {
  bool operator () (PeerConnectionBase* p1, PeerConnectionBase* p2) const {
    return *p1->peer_chunks()->download_throttle()->rate() > *p2->peer_chunks()->download_throttle()->rate();
  }
};

struct choke_manager_is_remote_not_uploading {
  bool operator () (PeerConnectionBase* p1) const {
    return *p1->peer_chunks()->download_throttle()->rate() == 0;
  }
};

struct choke_manager_is_remote_uploading {
  bool operator () (PeerConnectionBase* p1) const {
    return *p1->peer_chunks()->download_throttle()->rate() != 0;
  }
};

struct choke_manager_is_interested {
  bool operator () (PeerConnectionBase* p) const {
    return p->is_upload_wanted();
  }
};

struct choke_manager_not_recently_unchoked {
  bool operator () (PeerConnectionBase* p) const {
    return p->time_last_choked() + rak::timer::from_seconds(10) < cachedTime;
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

// 1  > 1
// 9  > 2
// 17 > 3 < 21
// 25 > 4 < 31
// 33 > 5 < 41
// 65 > 9 < 81

inline unsigned int
ChokeManager::max_alternate() const {
  if (m_currentlyUnchoked < 31)
    return (m_currentlyUnchoked + 7) / 8;
  else
    return (m_currentlyUnchoked + 9) / 10;
}

inline void
ChokeManager::alternate_ranges(iterator firstUnchoked, iterator lastUnchoked,
			       iterator firstChoked, iterator lastChoked,
			       unsigned int max) {
  max = std::min(std::min<unsigned int>(std::distance(firstUnchoked, lastUnchoked),
					std::distance(firstChoked, lastChoked)),
		 max);

  // Do unchoke first, then choke. Take the return value of the first
  // unchoke and use it for choking. Don't need the above min call.

  choke_range(firstUnchoked, lastUnchoked, max);
  unchoke_range(firstChoked, lastChoked, max);
}

inline void
ChokeManager::swap_with_shift(iterator first, iterator source) {
  while (first != source) {
    iterator tmp = source;
    std::iter_swap(tmp, --source);
  }
}

// Would propably replace maxUnchoked with whatever max the global
// manager gives us. When trying unchoke, we always ask the global
// manager for permission to unchoke, and when we did a choke.
//
// Don't notify the global manager anything when we keep the balance.

void
ChokeManager::balance() {
  // Return if no balance is needed.
  if (m_currentlyUnchoked == m_maxUnchoked)
    return;

  iterator beginUninterested = seperate_interested(m_connectionList->begin(), m_connectionList->end());
  iterator beginChoked       = seperate_unchoked(m_connectionList->begin(), beginUninterested);

  int adjust = m_maxUnchoked - m_currentlyUnchoked;

  if (adjust > 0) {
    adjust = unchoke_range(beginChoked, beginUninterested,
			   std::min((unsigned int)adjust, m_slotCanUnchoke()));

    m_slotUnchoke(adjust);

  } else if (adjust < 0)  {
    // We can do the choking before the slot is called as this
    // ChokeManager won't be unchoking the same peers due to the
    // call-back.
    adjust = choke_range(m_connectionList->begin(), beginChoked, -adjust);

    m_slotChoke(adjust);
  }
}

int
ChokeManager::cycle(unsigned int quota) {
  iterator lastChoked = seperate_interested(m_connectionList->begin(), m_connectionList->end());
  iterator firstChoked = seperate_unchoked(m_connectionList->begin(), lastChoked);

  // Partition away the connections we shall not choke.

  if (std::distance(m_connectionList->begin(), firstChoked) != (ConnectionList::difference_type)m_currentlyUnchoked)
    throw internal_error("ChokeManager::cycle() std::distance(m_connectionList->begin(), firstChoked) != m_currentlyUnchoked.");

  if (std::distance(m_connectionList->begin(), lastChoked) != (ConnectionList::difference_type)m_currentlyInterested)
    throw internal_error("ChokeManager::cycle() std::distance(m_connectionList->begin(), lastChoked) != m_currentlyInterested.");

  iterator firstUnchoked = m_connectionList->begin();
  iterator lastUnchoked = firstChoked;

  int cycled, adjust;

  // We don't call the resource manager slots, the number of un/choked
  // connections is returned.

  adjust = std::min(quota, m_maxUnchoked) - m_currentlyUnchoked;

  if (adjust > 0)
    firstChoked += (cycled = unchoke_range(firstChoked, lastChoked, adjust));
  else if (adjust < 0)
    firstUnchoked -= (cycled = -choke_range(firstUnchoked, lastUnchoked, -adjust));
  else
    cycled = 0;

  adjust = max_alternate() - std::abs(cycled);

  // Consider rolling this into the above calls.
  if (adjust > 0)
    alternate_ranges(firstUnchoked, lastUnchoked, firstChoked, lastChoked, adjust);

  if (m_currentlyUnchoked > quota)
    throw internal_error("ChokeManager::cycle() m_currentlyUnchoked > quota.");

  return cycled;
}

void
ChokeManager::set_interested(PeerConnectionBase* pc) {
  m_currentlyInterested++;

  if (!pc->is_up_choked())
    return;

  if (m_currentlyUnchoked < m_maxUnchoked &&
      pc->time_last_choked() + rak::timer::from_seconds(10) < cachedTime &&
      m_slotCanUnchoke()) {
    pc->receive_choke(false);

    m_currentlyUnchoked++;
    m_slotUnchoke(1);
  }
}

void
ChokeManager::set_not_interested(PeerConnectionBase* pc) {
  m_currentlyInterested--;

  if (pc->is_up_choked())
    return;

  pc->receive_choke(true);

  m_currentlyUnchoked--;
  m_slotChoke(1);
}

// We are no longer be in m_connectionList.
void
ChokeManager::disconnected(PeerConnectionBase* pc) {
  if (pc->is_upload_wanted())
    m_currentlyInterested--;

  if (!pc->is_up_choked()) {
    m_currentlyUnchoked--;
    m_slotChoke(1);
  }
}

unsigned int
ChokeManager::choke_range(iterator first, iterator last, unsigned int max) {
  max = std::min(max, (unsigned int)distance(first, last));

  std::sort(first, last, choke_manager_read_rate_increasing());

  iterator split;
  split = std::stable_partition(first, last, choke_manager_not_recently_unchoked());
  split = std::find_if(first, split, choke_manager_is_remote_uploading());

  std::sort(first, split, choke_manager_write_rate_increasing());

  std::for_each(first, first + max, std::bind2nd(std::mem_fun(&PeerConnectionBase::receive_choke), true));

  m_currentlyUnchoked -= max;
  return max;
}
  
unsigned int
ChokeManager::unchoke_range(iterator first, iterator last, unsigned int max) {
  std::sort(first, last, choke_manager_read_rate_decreasing());

  unsigned int count = 0;

  // Find the split between the ones that are uploading to us, and
  // those that arn't. When unchoking, circa every third unchoke is of
  // a connection in the list of those not uploading to us.
  //
  // Perhaps we should prefer those we are interested in?

  iterator split = std::find_if(first, last, choke_manager_is_remote_not_uploading());

  for ( ; count != max && first != last; count++, first++) {

    if (split != last &&
	(*(*first)->peer_chunks()->download_throttle()->rate() < 500 || ::random() % m_generousUnchokes == 0)) {
      // Use a random connection that is not uploading to us.
      std::iter_swap(split, split + ::random() % std::distance(split, last));
      swap_with_shift(first, split++);
    }
    
    (*first)->receive_choke(false);
  }

  m_currentlyUnchoked += count;
  return count;
}

}
