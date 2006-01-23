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
#include <limits>
#include <rak/functional.h>

#include "torrent/exceptions.h"
#include "download/choke_manager.h"
#include "download/download_main.h"

#include "resource_manager.h"

namespace torrent {

ResourceManager::~ResourceManager() {
  if (m_currentlyUnchoked != 0)
    throw internal_error("ResourceManager::~ResourceManager() called but m_currentlyUnchoked != 0.");
}

void
ResourceManager::insert(DownloadMain* d, uint16_t priority) {
  iterator itr = std::find_if(begin(), end(), rak::less(priority, rak::mem_ptr_ref(&value_type::first)));

  Base::insert(itr, value_type(priority, d));
}

void
ResourceManager::erase(DownloadMain* d) {
  iterator itr = std::find_if(begin(), end(), rak::equal(d, rak::mem_ptr_ref(&value_type::second)));

  if (itr != end())
    Base::erase(itr);
}

ResourceManager::iterator
ResourceManager::find(DownloadMain* d) {
  return std::find_if(begin(), end(), rak::equal(d, rak::mem_ptr_ref(&value_type::second)));
}

void
ResourceManager::set_priority(iterator itr, uint16_t pri) {
  if (itr->first == pri)
    return;

  DownloadMain* d = itr->second;

  Base::erase(itr);
  insert(d, pri);
}

// The choking choke manager won't updated it's count until after
// possibly multiple calls of this function.
void
ResourceManager::receive_choke(unsigned int num) {
  if (num > m_currentlyUnchoked)
    throw internal_error("ResourceManager::receive_choke(...) received an invalid value.");

  m_currentlyUnchoked -= num;

  // Do unchoking of other peers.
}

void
ResourceManager::receive_unchoke(unsigned int num) {
  m_currentlyUnchoked += num;

  if (m_maxUnchoked != 0 && (num > m_maxUnchoked || m_currentlyUnchoked > m_maxUnchoked))
    throw internal_error("ResourceManager::receive_unchoke(...) received an invalid value.");
}

unsigned int
ResourceManager::retrieve_can_unchoke() {
  if (m_maxUnchoked == 0)
    return std::numeric_limits<unsigned int>::max();

  else if (m_currentlyUnchoked < m_maxUnchoked)
    return m_maxUnchoked - m_currentlyUnchoked;

  else
    return 0;
}

void
ResourceManager::receive_tick() {
  unsigned int totalWeight = total_weight();

  balance_unchoked(totalWeight);
}

unsigned int
ResourceManager::total_weight() const {
  return std::for_each(begin(), end(), rak::accumulate((unsigned int)0, rak::mem_ptr_ref(&value_type::first))).result;
}

struct resource_manager_interested_increasing {
  bool operator () (const ResourceManager::value_type& v1, const ResourceManager::value_type& v2) const {
    return v1.second->choke_manager()->currently_interested() < v2.second->choke_manager()->currently_interested();
  }
};

void
ResourceManager::balance_unchoked(unsigned int weight) {
  if (m_maxUnchoked != 0) {
    unsigned int quota = m_maxUnchoked;

    // We put the downloads with fewest interested first so that those
    // with more interested will gain any unused slots from the
    // preceding downloads. Consider multiplying with priority.
    //
    // Consider skipping the leading zero interested downloads.
    sort(begin(), end(), resource_manager_interested_increasing());

    for (iterator itr = begin(); weight != 0 && itr != end(); ++itr) {
      m_currentlyUnchoked += itr->second->choke_manager()->cycle((quota * itr->first) / weight);
      quota -= itr->second->choke_manager()->currently_unchoked();
      weight -= itr->first;
    }

    if (weight != 0)
      throw internal_error("ResourceManager::balance_unchoked(...) weight did not reach zero.");

  } else {
    for (iterator itr = begin(); itr != end(); ++itr)
      m_currentlyUnchoked += itr->second->choke_manager()->cycle(std::numeric_limits<unsigned int>::max());
  }
}



}
