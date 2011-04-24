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

#include "choke_group.h"
#include "choke_queue.h"

// TODO: Put resource_manager_entry in a separate file.
#include "resource_manager.h"

#include "torrent/exceptions.h"
#include "download/download_main.h"

namespace torrent {

choke_group::choke_group() : m_first(NULL), m_last(NULL) { }

struct choke_group_upload_increasing {
  bool operator () (const resource_manager_entry& v1, const resource_manager_entry& v2) {
    return v1.c_download()->c_upload_choke_manager()->size_total() < v2.c_download()->c_upload_choke_manager()->size_total();
  }
};

struct choke_group_download_increasing {
  bool operator () (const resource_manager_entry& v1, const resource_manager_entry& v2) {
    return v1.c_download()->c_download_choke_manager()->size_total() < v2.c_download()->c_download_choke_manager()->size_total();
  }
};

int
choke_group::balance_upload_unchoked(unsigned int weight, unsigned int max_unchoked) {
  int change = 0;

  if (max_unchoked == 0) {
    for (resource_manager_entry* itr = m_first; itr != m_last; ++itr)
      change += itr->download()->upload_choke_manager()->cycle(std::numeric_limits<unsigned int>::max());

    return change;
  }

  unsigned int quota = max_unchoked;

  // We put the downloads with fewest interested first so that those
  // with more interested will gain any unused slots from the
  // preceding downloads. Consider multiplying with priority.
  //
  // Consider skipping the leading zero interested downloads. Though
  // that won't work as they need to choke peers once their priority
  // is turned off.
  std::sort(m_first, m_last, choke_group_upload_increasing());

  for (resource_manager_entry* itr = m_first; itr != m_last; ++itr) {
    choke_queue* cm = itr->download()->upload_choke_manager();

    change += cm->cycle(weight != 0 ? (quota * itr->priority()) / weight : 0);

    quota -= cm->size_unchoked();
    weight -= itr->priority();
  }

  if (weight != 0)
    throw internal_error("choke_group::balance_upload_unchoked(...) weight did not reach zero.");

  return change;
}

int
choke_group::balance_download_unchoked(unsigned int weight, unsigned int max_unchoked) {
  int change = 0;

  if (max_unchoked == 0) {
    for (resource_manager_entry* itr = m_first; itr != m_last; ++itr)
      change += itr->download()->download_choke_manager()->cycle(std::numeric_limits<unsigned int>::max());

    return change;
  }

  unsigned int quota = max_unchoked;

  // We put the downloads with fewest interested first so that those
  // with more interested will gain any unused slots from the
  // preceding downloads. Consider multiplying with priority.
  //
  // Consider skipping the leading zero interested downloads. Though
  // that won't work as they need to choke peers once their priority
  // is turned off.
  std::sort(m_first, m_last, choke_group_download_increasing());

  for (resource_manager_entry* itr = m_first; itr != m_last; ++itr) {
    choke_queue* cm = itr->download()->download_choke_manager();

    change += cm->cycle(weight != 0 ? (quota * itr->priority()) / weight : 0);

    quota -= cm->size_unchoked();
    weight -= itr->priority();
  }

  if (weight != 0)
    throw internal_error("choke_group::balance_download_unchoked(...) weight did not reach zero.");

  return change;
}

}
