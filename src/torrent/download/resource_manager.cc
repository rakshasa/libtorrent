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
#include <limits>
#include <rak/functional.h>

#include "torrent/exceptions.h"
#include "download/choke_manager.h"
#include "download/download_main.h"

#include "resource_manager.h"

namespace torrent {

ResourceManager::ResourceManager() :
  m_currentlyUploadUnchoked(0),
  m_currentlyDownloadUnchoked(0),
  m_maxUploadUnchoked(0),
  m_maxDownloadUnchoked(0)
{
  choke_base_type::push_back(choke_group());
}

ResourceManager::~ResourceManager() {
  if (m_currentlyUploadUnchoked != 0)
    throw internal_error("ResourceManager::~ResourceManager() called but m_currentlyUploadUnchoked != 0.");

  if (m_currentlyDownloadUnchoked != 0)
    throw internal_error("ResourceManager::~ResourceManager() called but m_currentlyDownloadUnchoked != 0.");
}

ResourceManager::iterator
ResourceManager::insert(const resource_manager_entry& entry) {
  // init

  return base_type::insert(find_group_end(entry.priority()), entry);
}

void
ResourceManager::erase(DownloadMain* d) {
  iterator itr = std::find_if(begin(), end(), rak::equal(d, std::mem_fun_ref(&value_type::download)));

  if (itr != end())
    base_type::erase(itr);
}

ResourceManager::iterator
ResourceManager::find(DownloadMain* d) {
  return std::find_if(begin(), end(), rak::equal(d, std::mem_fun_ref(&value_type::download)));
}

ResourceManager::iterator
ResourceManager::find_group_end(uint16_t group) {
  return std::find_if(begin(), end(), rak::less(group, std::mem_fun_ref(&value_type::group)));
}

void
ResourceManager::set_priority(iterator itr, uint16_t pri) {
  itr->set_priority(pri);
}

void
ResourceManager::set_group(iterator itr, uint16_t grp) {
  if (itr->group() == grp)
    return;

  resource_manager_entry entry = *itr;
  entry.set_group(grp);
  
  // move and set.

  base_type::erase(itr);
  base_type::insert(find_group_end(entry.priority()), entry);
}

void
ResourceManager::set_max_upload_unchoked(unsigned int m) {
  if (m > (1 << 16))
    throw input_error("Max unchoked must be between 0 and 2^16.");

  m_maxUploadUnchoked = m;
}

void
ResourceManager::set_max_download_unchoked(unsigned int m) {
  if (m > (1 << 16))
    throw input_error("Max unchoked must be between 0 and 2^16.");

  m_maxDownloadUnchoked = m;
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

  // Update these when inserting/erasing downloads.
  base_type::iterator       entry_itr = base_type::begin();
  choke_base_type::iterator group_itr = choke_base_type::begin();

  while (group_itr != choke_base_type::end()) {
    group_itr->set_first(&*entry_itr);
    entry_itr = std::find_if(entry_itr, end(), rak::less(std::distance(choke_base_type::begin(), group_itr),
                                                         std::mem_fun_ref(&value_type::group)));
    group_itr->set_last(&*entry_itr);
    group_itr++;
  }

  group_itr = choke_base_type::begin();

  while (group_itr != choke_base_type::end()) {
    m_currentlyUploadUnchoked   += group_itr->balance_upload_unchoked(totalWeight, m_maxUploadUnchoked);
    m_currentlyDownloadUnchoked += group_itr->balance_download_unchoked(totalWeight, m_maxDownloadUnchoked);
    group_itr++;
  }
}

unsigned int
ResourceManager::total_weight() const {
  return std::for_each(begin(), end(), rak::accumulate((unsigned int)0, std::mem_fun_ref(&value_type::priority))).result;
}

}
