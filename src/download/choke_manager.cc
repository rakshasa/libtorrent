// libTorrent - BitTorrent library
// Copyright (C) 2005-2007, Jari Sundell
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
#include <numeric>
#include <cstdlib>

#include "protocol/peer_connection_base.h"

#include "choke_manager.h"
#include "choke_manager_node.h"

namespace torrent {

void
choke_manager_erase(ChokeManager::container_type* container, PeerConnectionBase* pc) {
  ChokeManager::container_type::iterator itr = std::find_if(container->begin(), container->end(),
                                                            rak::equal(pc, rak::mem_ref(&ChokeManager::value_type::first)));

  if (itr == container->end())
    throw internal_error("choke_manager_remove(...) itr == m_unchoked.end().");

  *itr = container->back();
  container->pop_back();
}

ChokeManager::~ChokeManager() {
  if (m_unchoked.size() != 0)
    throw internal_error("ChokeManager::~ChokeManager() called but m_currentlyUnchoked != 0.");

  if (m_queued.size() != 0)
    throw internal_error("ChokeManager::~ChokeManager() called but m_currentlyQueued != 0.");
}

// 1  > 1
// 9  > 2
// 17 > 3 < 21
// 25 > 4 < 31
// 33 > 5 < 41
// 65 > 9 < 81

inline uint32_t
ChokeManager::max_alternate() const {
  if (m_unchoked.size() < 31)
    return (m_unchoked.size() + 7) / 8;
  else
    return (m_unchoked.size() + 9) / 10;
}

// Would propably replace maxUnchoked with whatever max the global
// manager gives us. When trying unchoke, we always ask the global
// manager for permission to unchoke, and when we did a choke.
//
// Don't notify the global manager anything when we keep the balance.

void
ChokeManager::balance() {
  // Return if no balancing is needed. Don't return if is_unlimited()
  // as we might have just changed the value and have interested that
  // can be unchoked.
  if (m_unchoked.size() == m_maxUnchoked)
    return;

  int adjust = m_maxUnchoked - m_unchoked.size();

  if (adjust > 0) {
    adjust = unchoke_range(m_queued.begin(), m_queued.end(),
			   std::min((uint32_t)adjust, m_slotCanUnchoke()));

    m_slotUnchoke(adjust);

  } else if (adjust < 0)  {
    // We can do the choking before the slot is called as this
    // ChokeManager won't be unchoking the same peers due to the
    // call-back.
    adjust = choke_range(m_unchoked.begin(), m_unchoked.end(), -adjust);

    m_slotUnchoke(-adjust);
  }
}

int
ChokeManager::cycle(uint32_t quota) {
  quota = std::min(quota, m_maxUnchoked);

  // Does this properly handle 'unlimited' quota?
  uint32_t oldSize  = m_unchoked.size();
  uint32_t unchoked = unchoke_range(m_queued.begin(), m_queued.end(),
                                    std::max<uint32_t>(m_unchoked.size() < quota ? quota - m_unchoked.size() : 0,
                                                       std::min(quota, max_alternate())));

  if (m_unchoked.size() > quota)
    choke_range(m_unchoked.begin(), m_unchoked.end() - unchoked, m_unchoked.size() - quota);

  if (m_unchoked.size() > quota)
    throw internal_error("ChokeManager::cycle() m_unchoked.size() > quota.");

  return m_unchoked.size() - oldSize;
}

void
ChokeManager::set_queued(PeerConnectionBase* pc, ChokeManagerNode* base) {
  if (base->queued() || base->unchoked())
    return;

  base->set_queued(true);

  if (base->snubbed())
    return;

  if ((m_flags & flag_unchoke_all_new || (!is_full() && m_slotCanUnchoke())) &&
      base->time_last_choke() + rak::timer::from_seconds(10) < cachedTime) {
    m_unchoked.push_back(value_type(pc, 0));
    m_slotConnection(pc, false);

    m_slotUnchoke(1);

  } else {
    m_queued.push_back(value_type(pc, 0));
  }
}

void
ChokeManager::set_not_queued(PeerConnectionBase* pc, ChokeManagerNode* base) {
  if (!base->queued())
    return;

  base->set_queued(false);

  if (base->snubbed())
    return;

  if (base->unchoked()) {
    choke_manager_erase(&m_unchoked, pc);
    m_slotConnection(pc, true);
    m_slotUnchoke(-1);

  } else {
    choke_manager_erase(&m_queued, pc);
  }
}

void
ChokeManager::set_snubbed(PeerConnectionBase* pc, ChokeManagerNode* base) {
  if (base->snubbed())
    return;

  base->set_snubbed(true);

  if (base->unchoked()) {
    choke_manager_erase(&m_unchoked, pc);
    m_slotConnection(pc, true);
    m_slotUnchoke(-1);

  } else if (base->queued()) {
    choke_manager_erase(&m_queued, pc);
  }

  base->set_queued(false);
}

void
ChokeManager::set_not_snubbed(PeerConnectionBase* pc, ChokeManagerNode* base) {
  if (!base->snubbed())
    return;

  base->set_snubbed(false);

  if (!base->queued())
    return;

  if (base->unchoked())
    throw internal_error("ChokeManager::set_not_snubbed(...) base->unchoked().");
  
  if ((m_flags & flag_unchoke_all_new || (!is_full() && m_slotCanUnchoke())) &&
      base->time_last_choke() + rak::timer::from_seconds(10) < cachedTime) {
    m_unchoked.push_back(value_type(pc, 0));
    m_slotConnection(pc, false);

    m_slotUnchoke(1);

  } else {
    m_queued.push_back(value_type(pc, 0));
  }
}

// We are no longer in m_connectionList.
void
ChokeManager::disconnected(PeerConnectionBase* pc, ChokeManagerNode* base) {
  if (base->snubbed()) {
    // Do nothing.

  } else if (base->unchoked()) {
    choke_manager_erase(&m_unchoked, pc);
    m_slotUnchoke(-1);

  } else if (base->queued()) {
    choke_manager_erase(&m_queued, pc);
  }

  base->set_queued(false);
}

struct choke_manager_less {
  bool operator () (ChokeManager::value_type v1, ChokeManager::value_type v2) const { return v1.second < v2.second; }
};

void
choke_manager_allocate_slots(ChokeManager::iterator first, ChokeManager::iterator last,
                             uint32_t max, uint32_t* weights, ChokeManager::target_type* target) {
  std::sort(first, last, choke_manager_less());

  // 'weightTotal' only contains the weight of targets that have
  // connections to unchoke. When all connections are in a group are
  // to be unchoked, then the group's weight is removed.
  uint32_t weightTotal = 0;
  uint32_t unchoke = max;

  target[0].second = first;

  for (uint32_t i = 0; i < ChokeManager::order_max_size; i++) {
    target[i].first = 0;
    target[i + 1].second = std::find_if(target[i].second, last,
                                        rak::less(i * ChokeManager::order_base + (ChokeManager::order_base - 1),
                                                  rak::mem_ref(&ChokeManager::value_type::second)));

    if (std::distance(target[i].second, target[i + 1].second) != 0)
      weightTotal += weights[i];
  }

  // Spread available unchoke slots as long as we can give everyone an
  // equal share.
  while (weightTotal != 0 && unchoke / weightTotal > 0) {
    uint32_t base = unchoke / weightTotal;

    for (uint32_t itr = 0; itr < ChokeManager::order_max_size; itr++) {
      uint32_t s = std::distance(target[itr].second, target[itr + 1].second);

      if (weights[itr] == 0 || target[itr].first >= s)
        continue;
      
      uint32_t u = std::min(s - target[itr].first, base * weights[itr]);

      unchoke -= u;
      target[itr].first += u;

      if (target[itr].first >= s)
        weightTotal -= weights[itr];
    }
  }

  // Spread the remainder starting from a random position based on the
  // total weight. This will ensure that aggregated over time we
  // spread the unchokes equally according to the weight table.
  if (weightTotal != 0 && unchoke != 0) {
    uint32_t start = ::random() % weightTotal;
    unsigned int itr = 0;

    for ( ; ; itr++) {
      uint32_t s = std::distance(target[itr].second, target[itr + 1].second);

      if (weights[itr] == 0 || target[itr].first >= s)
        continue;

      if (start < weights[itr])
        break;

      start -= weights[itr];
    }

    for ( ; weightTotal != 0 && unchoke != 0; itr = (itr + 1) % ChokeManager::order_max_size) {
      uint32_t s = std::distance(target[itr].second, target[itr + 1].second);

      if (weights[itr] == 0 || target[itr].first >= s)
        continue;

      uint32_t u = std::min(unchoke, std::min(s - target[itr].first, weights[itr] - start));

      start = 0;
      unchoke -= u;
      target[itr].first += u;

      if (target[itr].first >= s)
        weightTotal -= weights[itr];
    }
  }
}

uint32_t
ChokeManager::choke_range(iterator first, iterator last, uint32_t max) {
  m_slotChokeWeight(first, last);

  target_type target[order_max_size + 1];
  choke_manager_allocate_slots(first, last, max, m_chokeWeight, target);

  // Now do the actual unchoking.
  uint32_t count = 0;

  // Iterate in reverse so that the iterators in 'target' remain vaild
  // even though we remove random ranges.
  for (target_type* itr = target + order_max_size; itr != target; itr--) {
    if ((itr - 1)->first > (uint32_t)std::distance((itr - 1)->second, itr->second))
      throw internal_error("ChokeManager::choke_range(...) itr->first > std::distance((itr - 1)->second, itr->second).");

    if (itr->second - (itr - 1)->first > itr->second ||
        itr->second - (itr - 1)->first < m_unchoked.begin() ||
        itr->second - (itr - 1)->first > m_unchoked.end() ||
        (itr - 1)->second < m_unchoked.begin() ||
        (itr - 1)->second > m_unchoked.end())
      throw internal_error("ChokeManager::choke_range(...) bad iterator range.");

    count += (itr - 1)->first;

    // We move the connections that return true, while the ones that
    // return false get thrown out. The function called must update
    // ChunkManager::m_queued if false is returned.
    //
    // The C++ standard says std::partition will call the predicate
    // max 'last - first' times, so we can assume it gets called once
    // per element.
    iterator split = std::partition(itr->second - (itr - 1)->first, itr->second,
                                    rak::on(rak::mem_ref(&value_type::first), std::bind2nd(m_slotConnection, true)));

    m_queued.insert(m_queued.end(), itr->second - (itr - 1)->first, split);
    m_unchoked.erase(itr->second - (itr - 1)->first, itr->second);
  }

  if (count > max)
    throw internal_error("ChokeManager::choke_range(...) count > max.");

  return count;
}
  
uint32_t
ChokeManager::unchoke_range(iterator first, iterator last, uint32_t max) {
  m_slotUnchokeWeight(first, last);

  target_type target[order_max_size + 1];
  choke_manager_allocate_slots(first, last, max, m_unchokeWeight, target);

  // Now do the actual unchoking.
  uint32_t count = 0;

  for (target_type* itr = target + order_max_size; itr != target; itr--) {
    if ((itr - 1)->first > (uint32_t)std::distance((itr - 1)->second, itr->second))
      throw internal_error("ChokeManager::unchoke_range(...) itr->first > std::distance((itr - 1)->second, itr->second).");

    if (itr->second - (itr - 1)->first > itr->second ||
        itr->second - (itr - 1)->first < m_queued.begin() ||
        itr->second - (itr - 1)->first > m_queued.end() ||
        (itr - 1)->second < m_queued.begin() ||
        (itr - 1)->second > m_queued.end())
      throw internal_error("ChokeManager::unchoke_range(...) bad iterator range.");

    count += (itr - 1)->first;

    std::for_each(itr->second - (itr - 1)->first, itr->second,
                  rak::on(rak::mem_ref(&value_type::first), std::bind2nd(m_slotConnection, false)));

    m_unchoked.insert(m_unchoked.end(), itr->second - (itr - 1)->first, itr->second);
    m_queued.erase(itr->second - (itr - 1)->first, itr->second);
  }

  if (count > max)
    throw internal_error("ChokeManager::unchoke_range(...) count > max.");

  return count;
}

// Note that these algorithms fail if the rate >= 2^30.

// Need to add the recently unchoked check here?

uint32_t weights_upload_choke[ChokeManager::order_max_size]   = { 1, 1, 1, 1 };
uint32_t weights_upload_unchoke[ChokeManager::order_max_size] = { 1, 3, 9, 0 };

void
calculate_upload_choke(ChokeManager::iterator first, ChokeManager::iterator last) {
  while (first != last) {
    // Very crude version for now.
    uint32_t downloadRate = first->first->peer_chunks()->download_throttle()->rate()->rate();
    first->second = ChokeManager::order_base - 1 - downloadRate;

    first++;
  }
}

void
calculate_upload_unchoke(ChokeManager::iterator first, ChokeManager::iterator last) {
  while (first != last) {
    if (first->first->is_down_local_unchoked()) {
      uint32_t downloadRate = first->first->peer_chunks()->download_throttle()->rate()->rate();

      // If the peer transmits at less than 1KB, we should consider it
      // to be a rather stingy peer, and should look for new ones.

      if (downloadRate < 1000)
        first->second = downloadRate;
      else
        first->second = 2 * ChokeManager::order_base + downloadRate;

    } else {
      // This will be our optimistic unchoke queue, should be
      // semi-random. Give lower weights to known stingy peers.

      first->second = 1 * ChokeManager::order_base + ::random() % (1 << 10);
    }

    first++;
  }
}

// Fix this, but for now just use something simple.

uint32_t weights_download_choke[ChokeManager::order_max_size]   = { 1, 1, 1, 1 };
uint32_t weights_download_unchoke[ChokeManager::order_max_size] = { 1, 1, 1, 1 };

void
calculate_download_choke(ChokeManager::iterator first, ChokeManager::iterator last) {
  while (first != last) {
    // Very crude version for now.
    uint32_t downloadRate = first->first->peer_chunks()->download_throttle()->rate()->rate();
    first->second = ChokeManager::order_base - 1 - downloadRate;

    first++;
  }
}

void
calculate_download_unchoke(ChokeManager::iterator first, ChokeManager::iterator last) {
  while (first != last) {
    // Very crude version for now.
    uint32_t downloadRate = first->first->peer_chunks()->download_throttle()->rate()->rate();
    first->second = downloadRate;

    first++;
  }
}

}
