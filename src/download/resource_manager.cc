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
ResourceManager::insert(int priority, DownloadMain* d) {
  iterator itr = std::find_if(begin(), end(), rak::less(priority, rak::mem_ptr_ref(&value_type::first)));

  Base::insert(itr, value_type(priority, d));

  // Set unchoke quota etc. Based on what is remaining, calculate it
  // from the list.
  if (m_maxUnchoked == 0)
    d->choke_manager()->set_quota(std::numeric_limits<unsigned int>::max());
  else
    d->choke_manager()->set_quota(m_maxUnchoked - m_currentlyUnchoked);
}

void
ResourceManager::erase(DownloadMain* d) {
  iterator itr = std::find_if(begin(), end(), rak::equal(d, rak::mem_ptr_ref(&value_type::second)));

  if (itr != end())
    Base::erase(itr);
}

// The choking choke manager won't updated it's count until after
// possibly multiple calls of this function.
void
ResourceManager::receive_choke() {
  m_currentlyUnchoked--;
}

bool
ResourceManager::receive_unchoke() {
  m_currentlyUnchoked++;

  return true;
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

void
ResourceManager::balance_unchoked(unsigned int weight) {
  if (m_maxUnchoked != 0) {
    unsigned int quota = m_maxUnchoked;

    // Sort by interested, lowest first. Consider skipping those that
    // want zero.

    for (iterator itr = begin(); itr != end(); ++itr) {
      m_currentlyUnchoked += itr->second->choke_manager()->cycle((quota * itr->first) / weight);
      quota -= itr->second->choke_manager()->currently_unchoked();
    }

  } else {
    for (iterator itr = begin(); itr != end(); ++itr)
      itr->second->choke_manager()->cycle(std::numeric_limits<unsigned int>::max());
  }
}



}
