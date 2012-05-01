// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
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
#include <tr1/functional>
#include <limits>
#include <rak/functional.h>

#include "torrent/exceptions.h"
#include "torrent/download/choke_group.h"
#include "torrent/utils/log.h"
#include "download/download_main.h"
#include "protocol/peer_connection_base.h"

#include "choke_queue.h"
#include "resource_manager.h"

namespace tr1 { using namespace std::tr1; }

namespace torrent {

const Rate* resource_manager_entry::up_rate() const { return m_download->info()->up_rate(); }
const Rate* resource_manager_entry::down_rate() const { return m_download->info()->down_rate(); }

ResourceManager::ResourceManager() :
  m_currentlyUploadUnchoked(0),
  m_currentlyDownloadUnchoked(0),
  m_maxUploadUnchoked(0),
  m_maxDownloadUnchoked(0)
{
  choke_base_type::push_back(new choke_group());

  choke_base_type::back()->set_name("default");
  choke_base_type::back()->set_first(&*base_type::end());
  choke_base_type::back()->set_last(&*base_type::end());

  choke_base_type::back()->up_queue()->set_heuristics(choke_queue::HEURISTICS_UPLOAD_LEECH);
  choke_base_type::back()->down_queue()->set_heuristics(choke_queue::HEURISTICS_DOWNLOAD_LEECH);

  choke_base_type::back()->up_queue()->set_slot_unchoke(tr1::bind(&ResourceManager::receive_upload_unchoke, this, tr1::placeholders::_1));
  choke_base_type::back()->down_queue()->set_slot_unchoke(tr1::bind(&ResourceManager::receive_download_unchoke, this, tr1::placeholders::_1));
  choke_base_type::back()->up_queue()->set_slot_can_unchoke(tr1::bind(&ResourceManager::retrieve_upload_can_unchoke, this));
  choke_base_type::back()->down_queue()->set_slot_can_unchoke(tr1::bind(&ResourceManager::retrieve_download_can_unchoke, this));
  choke_base_type::back()->up_queue()->set_slot_connection(tr1::bind(&PeerConnectionBase::receive_upload_choke, tr1::placeholders::_1, tr1::placeholders::_2));
  choke_base_type::back()->down_queue()->set_slot_connection(tr1::bind(&PeerConnectionBase::receive_download_choke, tr1::placeholders::_1, tr1::placeholders::_2));
}

ResourceManager::~ResourceManager() {
  if (m_currentlyUploadUnchoked != 0)
    throw internal_error("ResourceManager::~ResourceManager() called but m_currentlyUploadUnchoked != 0.");

  if (m_currentlyDownloadUnchoked != 0)
    throw internal_error("ResourceManager::~ResourceManager() called but m_currentlyDownloadUnchoked != 0.");

  std::for_each(choke_base_type::begin(), choke_base_type::end(), rak::call_delete<choke_group>());
}

// If called directly ensure a valid group has been selected.
ResourceManager::iterator
ResourceManager::insert(const resource_manager_entry& entry) {
  iterator itr = base_type::insert(base_type::end(), entry);

  DownloadMain* download = itr->download();

  download->set_choke_group(choke_base_type::at(entry.group()));

  update_group_iterators();

  choke_queue::move_connections(NULL, download->choke_group()->up_queue(), download, download->up_group_entry());
  choke_queue::move_connections(NULL, download->choke_group()->down_queue(), download, download->down_group_entry());

  return itr;
}

void
ResourceManager::update_group_iterators() {
  base_type::iterator       entry_itr = base_type::begin();
  choke_base_type::iterator group_itr = choke_base_type::begin();

  while (group_itr != choke_base_type::end()) {
    (*group_itr)->set_first(&*entry_itr);
    entry_itr = std::find_if(entry_itr, end(), rak::less(std::distance(choke_base_type::begin(), group_itr),
                                                         std::mem_fun_ref(&value_type::group)));
    (*group_itr)->set_last(&*entry_itr);
    group_itr++;
  }
}

void
ResourceManager::validate_group_iterators() {
  base_type::iterator       entry_itr = base_type::begin();
  choke_base_type::iterator group_itr = choke_base_type::begin();

  while (group_itr != choke_base_type::end()) {
    if ((*group_itr)->first() != &*entry_itr)
      throw internal_error("ResourceManager::receive_tick() invalid first iterator.");

    entry_itr = std::find_if(entry_itr, end(), rak::less(std::distance(choke_base_type::begin(), group_itr),
                                                         std::mem_fun_ref(&value_type::group)));
    if ((*group_itr)->last() != &*entry_itr)
      throw internal_error("ResourceManager::receive_tick() invalid last iterator.");

    group_itr++;
  }
}

void
ResourceManager::erase(DownloadMain* d) {
  iterator itr = std::find_if(begin(), end(), rak::equal(d, std::mem_fun_ref(&value_type::download)));

  if (itr == end())
    throw internal_error("ResourceManager::erase() itr == end().");

  choke_queue::move_connections(d->choke_group()->up_queue(), NULL, d, d->up_group_entry());
  choke_queue::move_connections(d->choke_group()->down_queue(), NULL, d, d->down_group_entry());

  choke_base_type::iterator group_itr = choke_base_type::begin() + itr->group();
  (*group_itr)->set_last((*group_itr)->last() - 1);

  base_type::erase(itr);
}

ResourceManager::iterator
ResourceManager::find(DownloadMain* d) {
  return std::find_if(begin(), end(), rak::equal(d, std::mem_fun_ref(&value_type::download)));
}

ResourceManager::iterator
ResourceManager::find_throw(DownloadMain* d) {
  iterator itr = std::find_if(begin(), end(), rak::equal(d, std::mem_fun_ref(&value_type::download)));

  if (itr == end())
    throw input_error("Could not find download in resource manager.");

  return itr;
}

void
ResourceManager::set_priority(iterator itr, uint16_t pri) {
  itr->set_priority(pri);
}

void
ResourceManager::set_group(iterator itr, uint16_t grp) {
  if (itr->group() == grp)
    return;

  //  if (grp >= choke_base_type::size())
    throw input_error("Choke group not found.");
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
  lt_log_print(LOG_PEER_INFO, "Upload unchoked slots adjust; currently:%u adjust:%i", m_currentlyUploadUnchoked, num);

  if ((int)m_currentlyUploadUnchoked + num < 0)
    throw internal_error("ResourceManager::receive_upload_unchoke(...) received an invalid value.");

  m_currentlyUploadUnchoked += num;
}

void
ResourceManager::receive_download_unchoke(int num) {
  lt_log_print(LOG_PEER_INFO, "Download unchoked slots adjust; currently:%u adjust:%i", m_currentlyDownloadUnchoked, num);

  if ((int)m_currentlyDownloadUnchoked + num < 0)
    throw internal_error("ResourceManager::receive_download_unchoke(...) received an invalid value.");

  m_currentlyDownloadUnchoked += num;
}

int
ResourceManager::retrieve_upload_can_unchoke() {
  if (m_maxUploadUnchoked == 0)
    return std::numeric_limits<int>::max();

  return (int)m_maxUploadUnchoked - (int)m_currentlyUploadUnchoked;
}

int
ResourceManager::retrieve_download_can_unchoke() {
  if (m_maxDownloadUnchoked == 0)
    return std::numeric_limits<int>::max();

  return (int)m_maxDownloadUnchoked - (int)m_currentlyDownloadUnchoked;
}

void
ResourceManager::receive_tick() {
  validate_group_iterators();

  m_currentlyUploadUnchoked   += balance_unchoked(choke_base_type::size(), m_maxUploadUnchoked, true);
  m_currentlyDownloadUnchoked += balance_unchoked(choke_base_type::size(), m_maxDownloadUnchoked, false);

  if (m_currentlyUploadUnchoked != choke_base_type::back()->up_queue()->size_unchoked())
    throw torrent::internal_error("m_currentlyUploadUnchoked != choke_base_type::back()->up_queue()->size_unchoked()");

  if (m_currentlyDownloadUnchoked != choke_base_type::back()->down_queue()->size_unchoked())
    throw torrent::internal_error("m_currentlyDownloadUnchoked != choke_base_type::back()->down_queue()->size_unchoked()");
}

unsigned int
ResourceManager::total_weight() const {
  // TODO: This doesn't take into account inactive downloads.
  return std::for_each(begin(), end(), rak::accumulate((unsigned int)0, std::mem_fun_ref(&value_type::priority))).result;
}

int
ResourceManager::balance_unchoked(unsigned int weight, unsigned int max_unchoked, bool is_up) {
  int change = 0;

  if (max_unchoked == 0)
    max_unchoked = std::numeric_limits<unsigned int>::max();

  if (is_up) {
    change += choke_base_type::back()->up_queue()->cycle(max_unchoked);
    lt_log_print(LOG_PEER_DEBUG, "Upload unchoked slots cycle; currently:%u adjusted:%i max_unchoked:%u", m_currentlyUploadUnchoked, change, max_unchoked);
  } else {
    change += choke_base_type::back()->down_queue()->cycle(max_unchoked);
    lt_log_print(LOG_PEER_DEBUG, "Download unchoked slots cycle; currently:%u adjusted:%i max_unchoked:%u", m_currentlyDownloadUnchoked, change, max_unchoked);
  }

  return change;
}

}
