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
#include <numeric>
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
  quota = std::min(quota, m_maxUnchoked);

  unsigned int oldSize = m_unchoked.size();

  // This needs to consider the case when we don't really have
  // anything to unchoke later.
  int choke = std::max((int)m_unchoked.size() - (int)quota,
                       std::min<int>(max_alternate(), m_interested.size()));

  if (choke <= 0)
    choke = 0;
  else
    choke = choke_range(m_unchoked.begin(), m_unchoked.end(), choke);

  if (m_unchoked.size() < quota)
    unchoke_range(m_interested.begin(), m_interested.end() - choke, quota - m_unchoked.size());

  if (m_unchoked.size() > quota)
    throw internal_error("ChokeManager::cycle() m_unchoked.size() > quota.");

  return m_unchoked.size() - oldSize;
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
    m_unchoked.push_back(value_type(pc, 0));
    pc->receive_choke(false);

    m_slotUnchoke(1);

  } else {
    m_interested.push_back(value_type(pc, 0));
  }
}

void
choke_manager_erase(ChokeManager::container_type* container, PeerConnectionBase* pc) {
  ChokeManager::container_type::iterator itr = std::find_if(container->begin(), container->end(),
                                                            rak::equal(pc, rak::mem_ref(&ChokeManager::value_type::first)));

  if (itr == container->end())
    throw internal_error("choke_manager_remove(...) itr == m_unchoked.end().");

  *itr = container->back();
  container->pop_back();
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
    m_unchoked.push_back(value_type(pc, 0));
    pc->receive_choke(false);

    m_slotUnchoke(1);

  } else {
    m_interested.push_back(value_type(pc, 0));
  }
}

// We are no longer in m_connectionList.
void
ChokeManager::disconnected(PeerConnectionBase* pc) {
  if (!pc->is_up_choked()) {
    choke_manager_erase(&m_unchoked, pc);
    m_slotChoke(1);

  } else if (pc->is_upload_wanted()) {
    choke_manager_erase(&m_interested, pc);
  }
}

struct choke_manager_less {
  bool operator () (ChokeManager::value_type v1, ChokeManager::value_type v2) const { return v1.second < v2.second; }
};

struct choke_manager_greater {
  bool operator () (ChokeManager::value_type v1, ChokeManager::value_type v2) const { return v1.second > v2.second; }
};

// This could be handled more efficiently with only nth_element
// sorting, etc.
//
// Also, this should also swap the order the elements are sorted into,
// so we can grab them from the end of m_unchoked.
unsigned int
ChokeManager::choke_range(iterator first, iterator last, unsigned int max) {
  max = std::min<unsigned int>(max, distance(first, last));

  m_slotChokeWeight(first, last);
  std::sort(first, last, choke_manager_greater());

  std::for_each(last - max, last, rak::on(rak::mem_ref(&value_type::first),
                                          std::bind2nd(std::mem_fun(&PeerConnectionBase::receive_choke), true)));

  m_interested.insert(m_interested.end(), last - max, last);
  m_unchoked.erase(last - max, last);

  return max;
}
  
unsigned int
ChokeManager::unchoke_range(iterator first, iterator last, unsigned int max) {
  m_slotUnchokeWeight(first, last);
  std::sort(first, last, choke_manager_less());

  // Weighting and randomizing code, cleanup needed.
  static const uint32_t order_max_size = 4;
  static const uint32_t m_unchokeWeight[4 + 1] = { 0, 1, 3, 0 };

  // 'weightTotal' only contains the weight of targets that have
  // connections to unchoke. When all connections are in a group are
  // to be unchoked, then the group's weight is removed.
  uint32_t weightTotal = 0;
  uint32_t unchoke = max;

  std::pair<uint32_t, iterator> target[order_max_size + 1];

  target[0].second = first;

  for (uint32_t i = 0; i < order_max_size; i++) {
    target[i].first = 0;
    target[i + 1].second = std::find_if(target[i].second, last, rak::less(i * order_base + (order_base - 1), rak::mem_ref(&value_type::second)));

    if (std::distance(target[i].second, target[i + 1].second) != 0)
      weightTotal += m_unchokeWeight[i];
  }

  // Spread available unchoke slots as long as we can give everyone an
  // equal share.
  while (weightTotal != 0 && unchoke / weightTotal > 0) {
    uint32_t base = unchoke / weightTotal;

    for (uint32_t itr = 0; itr < order_max_size; itr++) {
      uint32_t s = std::distance(target[itr].second, target[itr + 1].second);

      if (m_unchokeWeight[itr] == 0 ||target[itr].first >= s)
        continue;
        
      uint32_t u = std::min(s - target[itr].first, base * m_unchokeWeight[itr]);

      unchoke -= u;
      target[itr].first += u;

      if (target[itr].first >= s)
        weightTotal -= m_unchokeWeight[itr];
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

      if (m_unchokeWeight[itr] == 0 || target[itr].first >= s)
        continue;

      if (start < m_unchokeWeight[itr])
        break;

      start -= m_unchokeWeight[itr];
    }

    for ( ; weightTotal != 0 && unchoke != 0; itr = (itr + 1) % order_max_size) {
      uint32_t s = std::distance(target[itr].second, target[itr + 1].second);

      if (m_unchokeWeight[itr] == 0 || target[itr].first >= s)
        continue;

      uint32_t u = std::min(unchoke, std::min(s - target[itr].first, m_unchokeWeight[itr] - start));

      start = 0;
      unchoke -= u;
      target[itr].first += u;

      if (target[itr].first >= s)
        weightTotal -= m_unchokeWeight[itr];
    }
  }

  // Now do the actual unchoking.
  unchoke = 0;

  for (std::pair<uint32_t, iterator>* itr = target + order_max_size; itr != target; itr--) {
    if ((itr - 1)->first > (uint32_t)std::distance((itr - 1)->second, itr->second))
      throw internal_error("ChokeManager::unchoke_range(...) itr->first > std::distance((itr - 1)->second, itr->second).");

    if (itr->second - (itr - 1)->first > itr->second ||
        itr->second - (itr - 1)->first < m_interested.begin() ||
        itr->second - (itr - 1)->first > m_interested.end() ||
        (itr - 1)->second < m_interested.begin() ||
        (itr - 1)->second > m_interested.end())
      throw internal_error("ChokeManager::unchoke_range(...) bad iterator range.");

    unchoke += (itr - 1)->first;

    std::for_each(itr->second - (itr - 1)->first, itr->second, rak::on(rak::mem_ref(&value_type::first),
                                                                       std::bind2nd(std::mem_fun(&PeerConnectionBase::receive_choke), false)));

    m_unchoked.insert(m_unchoked.end(), itr->second - (itr - 1)->first, itr->second);
    m_interested.erase(itr->second - (itr - 1)->first, itr->second);
  }

  if (unchoke > max)
    throw internal_error("ChokeManager::unchoke_range(...) unchoke > max.");

  return unchoke;
}

// Note that these algorithms fail if the rate >= 2^30.

// Need to add the recently unchoked check here?

void
calculate_upload_choke(ChokeManager::iterator first, ChokeManager::iterator last) {
  while (first != last) {
    if (!first->first->is_down_choked()) {
      uint32_t downloadRate = first->first->peer_chunks()->download_throttle()->rate()->rate();

      // If the peer transmits at less than 1KB, we should consider it
      // to be a rather stingy peer, and should look for new ones.

      if (downloadRate < 1000)
        first->second = downloadRate;
      else
        first->second = 2 * ChokeManager::order_base + downloadRate;

    } else {
      // The peer hasn't unchoked us yet, let us see if uploading some
      // more helps, so try to stay unchoked if other very slow
      // uploaders are unchoked.
      //
      // This should also check how long the peer's been unchoked. If
      // more than f.ex 240 seconds, we might as well give up on them
      // and bump them to order 0.

      first->second = 1 * ChokeManager::order_base + first->first->peer_chunks()->upload_throttle()->rate()->rate();
    }

    first++;
  }
}

void
calculate_upload_unchoke(ChokeManager::iterator first, ChokeManager::iterator last) {
  while (first != last) {
    if (!first->first->is_down_choked()) {
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

}
