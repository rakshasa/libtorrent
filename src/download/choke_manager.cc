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
  if (m_unchoked.size() != 0)
    throw internal_error("ChokeManager::~ChokeManager() called but m_currentlyUnchoked != 0.");

  if (m_interested.size() != 0)
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

// 1  > 1
// 9  > 2
// 17 > 3 < 21
// 25 > 4 < 31
// 33 > 5 < 41
// 65 > 9 < 81

inline unsigned int
ChokeManager::max_alternate() const {
  if (m_unchoked.size() < 31)
    return (m_unchoked.size() + 7) / 8;
  else
    return (m_unchoked.size() + 9) / 10;
}

inline void
ChokeManager::alternate_ranges(unsigned int max) {
  max = std::min(max, std::min<unsigned int>(m_unchoked.size(), m_interested.size()));

  // Do unchoke first, then choke. Take the return value of the first
  // unchoke and use it for choking. Don't need the above min call.
  unsigned int sizeInterested = m_interested.size();

  choke_range(m_unchoked.begin(), m_unchoked.end(), max);
  unchoke_range(m_interested.begin(), m_interested.begin() + sizeInterested, max);
}

// Would propably replace maxUnchoked with whatever max the global
// manager gives us. When trying unchoke, we always ask the global
// manager for permission to unchoke, and when we did a choke.
//
// Don't notify the global manager anything when we keep the balance.

void
ChokeManager::balance() {
  // Return if no balancing is needed.
  if (m_unchoked.size() == m_maxUnchoked)
    return;

  int adjust = m_maxUnchoked - m_unchoked.size();

  if (adjust > 0) {
    adjust = unchoke_range(m_interested.begin(), m_interested.end(),
			   std::min((unsigned int)adjust, m_slotCanUnchoke()));

    m_slotUnchoke(adjust);

  } else if (adjust < 0)  {
    // We can do the choking before the slot is called as this
    // ChokeManager won't be unchoking the same peers due to the
    // call-back.
    adjust = choke_range(m_unchoked.begin(), m_unchoked.end(), -adjust);

    m_slotChoke(adjust);
  }
}

int
ChokeManager::cycle(unsigned int quota) {
  // We don't call the resource manager slots, the number of un/choked
  // connections is returned.

  int cycled;
  int adjust = std::min(quota, m_maxUnchoked) - m_unchoked.size();

  if (adjust > 0)
    cycled = unchoke_range(m_interested.begin(), m_interested.end(), adjust);
  else if (adjust < 0)
    cycled = -choke_range(m_unchoked.begin(), m_unchoked.end(), -adjust);
  else
    cycled = 0;

  adjust = max_alternate() - std::abs(cycled);

  // Consider rolling this into the above calls.
  if (adjust > 0)
    alternate_ranges(adjust);

  if (m_unchoked.size() > quota)
    throw internal_error("ChokeManager::cycle() m_unchoked.size() > quota.");

  return cycled;
}

void
ChokeManager::set_interested(PeerConnectionBase* pc) {
  if (pc->is_down_interested())
    return;

  if (!pc->is_up_choked())
    throw internal_error("ChokeManager::set_interested(...) !pc->is_up_choked().");

  pc->set_down_interested(true);

  if (pc->peer_chunks()->is_snubbed())
    return;    

  if (m_unchoked.size() < m_maxUnchoked &&
      pc->time_last_choked() + rak::timer::from_seconds(10) < cachedTime &&
      m_slotCanUnchoke()) {
    m_unchoked.push_back(pc);
    pc->receive_choke(false);

    m_slotUnchoke(1);

  } else {
    m_interested.push_back(pc);
  }
}

void
choke_manager_erase(ChokeManager::container_type* container, PeerConnectionBase* pc) {
  ChokeManager::container_type::iterator itr = std::find(container->begin(), container->end(), pc);

  if (itr == container->end())
    throw internal_error("choke_manager_remove(...) itr == m_unchoked.end().");

  container->erase(itr);
}

void
ChokeManager::set_not_interested(PeerConnectionBase* pc) {
  if (!pc->is_down_interested())
    return;

  pc->set_down_interested(false);

  if (pc->peer_chunks()->is_snubbed())
    return;

  if (!pc->is_up_choked()) {
    choke_manager_erase(&m_unchoked, pc);
    pc->receive_choke(true);
    m_slotChoke(1);

  } else {
    choke_manager_erase(&m_interested, pc);
  }
}

void
ChokeManager::set_snubbed(PeerConnectionBase* pc) {
  if (pc->peer_chunks()->is_snubbed())
    return;

  pc->peer_chunks()->set_snubbed(true);

  if (!pc->is_up_choked()) {
    choke_manager_erase(&m_unchoked, pc);
    pc->receive_choke(true);
    m_slotChoke(1);

  } else if (pc->is_down_interested()) {
    choke_manager_erase(&m_interested, pc);
  }
}

void
ChokeManager::set_not_snubbed(PeerConnectionBase* pc) {
  if (!pc->peer_chunks()->is_snubbed())
    return;

  pc->peer_chunks()->set_snubbed(false);

  if (!pc->is_down_interested())
    return;

  if (!pc->is_up_choked())
    throw internal_error("ChokeManager::set_not_snubbed(...) !pc->is_up_choked().");
  
  if (m_unchoked.size() < m_maxUnchoked &&
      pc->time_last_choked() + rak::timer::from_seconds(10) < cachedTime &&
      m_slotCanUnchoke()) {
    m_unchoked.push_back(pc);
    pc->receive_choke(false);

    m_slotUnchoke(1);

  } else {
    m_interested.push_back(pc);
  }
}

// We are no longer in m_connectionList.
void
ChokeManager::disconnected(PeerConnectionBase* pc) {
  if (!pc->is_up_choked()) {
    iterator itr = std::find(m_unchoked.begin(), m_unchoked.end(), pc);

    if (itr == m_unchoked.end())
      throw internal_error("ChokeManager::disconnected(...) itr == m_unchoked.end().");

    m_unchoked.erase(itr);

    m_slotChoke(1);

  } else if (pc->is_upload_wanted()) {
    iterator itr = std::find(m_interested.begin(), m_interested.end(), pc);

    if (itr == m_interested.end())
      throw internal_error("ChokeManager::disconnected(...) itr == m_interested.end().");

    m_interested.erase(itr);
  }
}

// This could be handled more efficiently with only nth_element
// sorting, etc.
//
// Also, this should also swap the order the elements are sorted into,
// so we can grab them from the end of m_unchoked.
unsigned int
ChokeManager::choke_range(iterator first, iterator last, unsigned int max) {
  max = std::min<unsigned int>(max, distance(first, last));

  std::sort(first, last, choke_manager_read_rate_increasing());

  iterator split;
  split = std::stable_partition(first, last, choke_manager_not_recently_unchoked());
  split = std::find_if(first, split, choke_manager_is_remote_uploading());

  std::sort(first, split, choke_manager_write_rate_increasing());

  std::for_each(first, first + max, std::bind2nd(std::mem_fun(&PeerConnectionBase::receive_choke), true));

  m_interested.insert(m_interested.end(), first, first + max);
  m_unchoked.erase(first, first + max);

  return max;
}
  
inline void
ChokeManager::swap_with_shift(iterator first, iterator source) {
  while (first != source) {
    iterator tmp = source;
    std::iter_swap(tmp, --source);
  }
}

unsigned int
ChokeManager::unchoke_range(iterator first, iterator last, unsigned int max) {
  std::sort(first, last, choke_manager_read_rate_decreasing());

  // Find the split between the ones that are uploading to us, and
  // those that arn't. When unchoking, circa every third unchoke is of
  // a connection in the list of those not uploading to us.
  //
  // Perhaps we should prefer those we are interested in?

  iterator itr = first;
  iterator split = std::find_if(first, last, choke_manager_is_remote_not_uploading());

  for ( ; (unsigned int)std::distance(first, itr) != max && itr != last; itr++) {

    if (split != last &&
	(*(*itr)->peer_chunks()->download_throttle()->rate() < 500 || ::random() % m_generousUnchokes == 0)) {
      // Use a random connection that is not uploading to us.
      std::iter_swap(split, split + ::random() % std::distance(split, last));
      swap_with_shift(itr, split++);
    }
    
    (*itr)->receive_choke(false);
  }

  m_unchoked.insert(m_unchoked.end(), first, itr);
  m_interested.erase(first, itr);

  return std::distance(first, itr);
}

}
