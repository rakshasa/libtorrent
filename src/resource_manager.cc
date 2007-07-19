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
#include <limits>
#include <rak/functional.h>

#include "torrent/exceptions.h"
#include "download/choke_manager.h"
#include "download/download_main.h"

#include "resource_manager.h"

namespace torrent {

ResourceManager::~ResourceManager() {
  if (m_currentlyUploadUnchoked != 0)
    throw internal_error("ResourceManager::~ResourceManager() called but m_currentlyUploadUnchoked != 0.");

  if (m_currentlyDownloadUnchoked != 0)
    throw internal_error("ResourceManager::~ResourceManager() called but m_currentlyDownloadUnchoked != 0.");
}

void
ResourceManager::insert(DownloadMain* d, uint16_t priority) {
  iterator itr = std::find_if(begin(), end(), rak::less(priority, rak::mem_ref(&value_type::first)));

  base_type::insert(itr, value_type(priority, d));
}

void
ResourceManager::erase(DownloadMain* d) {
  iterator itr = std::find_if(begin(), end(), rak::equal(d, rak::mem_ref(&value_type::second)));

  if (itr != end())
    base_type::erase(itr);
}

ResourceManager::iterator
ResourceManager::find(DownloadMain* d) {
  return std::find_if(begin(), end(), rak::equal(d, rak::mem_ref(&value_type::second)));
}

void
ResourceManager::set_priority(iterator itr, uint16_t pri) {
  if (itr->first == pri)
    return;

  DownloadMain* d = itr->second;

  base_type::erase(itr);
  insert(d, pri);
}

// The choking choke manager won't updated it's count until after
// possibly multiple calls of this function.
void
ResourceManager::receive_upload_unchoke(int num) {
  if ((int)m_currentlyUploadUnchoked + num < 0)
    throw internal_error("ResourceManager::receive_upload_unchoke(...) received an invalid value.");

  m_currentlyUploadUnchoked += num;
}

void
ResourceManager::receive_download_unchoke(int num) {
  if ((int)m_currentlyDownloadUnchoked + num < 0)
    throw internal_error("ResourceManager::receive_download_unchoke(...) received an invalid value.");

  m_currentlyDownloadUnchoked += num;
}

unsigned int
ResourceManager::retrieve_upload_can_unchoke() {
  if (m_maxUploadUnchoked == 0)
    return std::numeric_limits<unsigned int>::max();

  else if (m_currentlyUploadUnchoked < m_maxUploadUnchoked)
    return m_maxUploadUnchoked - m_currentlyUploadUnchoked;

  else
    return 0;
}

unsigned int
ResourceManager::retrieve_download_can_unchoke() {
  if (m_maxDownloadUnchoked == 0)
    return std::numeric_limits<unsigned int>::max();

  else if (m_currentlyDownloadUnchoked < m_maxDownloadUnchoked)
    return m_maxDownloadUnchoked - m_currentlyDownloadUnchoked;

  else
    return 0;
}

void
ResourceManager::receive_tick() {
  unsigned int totalWeight = total_weight();

  balance_upload_unchoked(totalWeight);
  balance_download_unchoked(totalWeight);
}

unsigned int
ResourceManager::total_weight() const {
  return std::for_each(begin(), end(), rak::accumulate((unsigned int)0, rak::const_mem_ref(&value_type::first))).result;
}

struct resource_manager_upload_increasing {
  bool operator () (const ResourceManager::value_type& v1, const ResourceManager::value_type& v2) const {
    return v1.second->upload_choke_manager()->size_total() < v2.second->upload_choke_manager()->size_total();
  }
};

struct resource_manager_download_increasing {
  bool operator () (const ResourceManager::value_type& v1, const ResourceManager::value_type& v2) const {
    return v1.second->download_choke_manager()->size_total() < v2.second->download_choke_manager()->size_total();
  }
};

void
ResourceManager::balance_upload_unchoked(unsigned int weight) {
  if (m_maxUploadUnchoked != 0) {
    unsigned int quota = m_maxUploadUnchoked;

    // We put the downloads with fewest interested first so that those
    // with more interested will gain any unused slots from the
    // preceding downloads. Consider multiplying with priority.
    //
    // Consider skipping the leading zero interested downloads. Though
    // that won't work as they need to choke peers once their priority
    // is turned off.
    sort(begin(), end(), resource_manager_upload_increasing());

    for (iterator itr = begin(); itr != end(); ++itr) {
      ChokeManager* cm = itr->second->upload_choke_manager();

      m_currentlyUploadUnchoked += cm->cycle(weight != 0 ? (quota * itr->first) / weight : 0);

      quota -= cm->size_unchoked();
      weight -= itr->first;
    }

    if (weight != 0)
      throw internal_error("ResourceManager::balance_upload_unchoked(...) weight did not reach zero.");

  } else {
    for (iterator itr = begin(); itr != end(); ++itr)
      m_currentlyUploadUnchoked += itr->second->upload_choke_manager()->cycle(std::numeric_limits<unsigned int>::max());
  }
}

void
ResourceManager::balance_download_unchoked(unsigned int weight) {
  if (m_maxDownloadUnchoked != 0) {
    unsigned int quota = m_maxDownloadUnchoked;

    // We put the downloads with fewest interested first so that those
    // with more interested will gain any unused slots from the
    // preceding downloads. Consider multiplying with priority.
    //
    // Consider skipping the leading zero interested downloads. Though
    // that won't work as they need to choke peers once their priority
    // is turned off.
    sort(begin(), end(), resource_manager_download_increasing());

    for (iterator itr = begin(); itr != end(); ++itr) {
      ChokeManager* cm = itr->second->download_choke_manager();

      m_currentlyDownloadUnchoked += cm->cycle(weight != 0 ? (quota * itr->first) / weight : 0);

      quota -= cm->size_unchoked();
      weight -= itr->first;
    }

    if (weight != 0)
      throw internal_error("ResourceManager::balance_download_unchoked(...) weight did not reach zero.");

  } else {
    for (iterator itr = begin(); itr != end(); ++itr)
      m_currentlyDownloadUnchoked += itr->second->download_choke_manager()->cycle(std::numeric_limits<unsigned int>::max());
  }
}

}
