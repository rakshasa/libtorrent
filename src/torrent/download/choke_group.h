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

#ifndef LIBTORRENT_DOWNLOAD_CHOKE_GROUP_H
#define LIBTORRENT_DOWNLOAD_CHOKE_GROUP_H

#include <string>
#include <vector>
#include <inttypes.h>
#include <torrent/common.h>
#include <torrent/download/choke_queue.h>

// TODO: Separate out resource_manager_entry.
#include <torrent/download/resource_manager.h>

namespace torrent {

class choke_queue;
class resource_manager_entry;

class LIBTORRENT_EXPORT choke_group {
public:
  enum tracker_mode_enum {
    TRACKER_MODE_NORMAL,
    TRACKER_MODE_AGGRESSIVE
  };

  choke_group();
  
  const std::string&  name() const { return m_name; }
  void                set_name(const std::string& name) { m_name = name; }

  tracker_mode_enum   tracker_mode() const                   { return m_tracker_mode; }
  void                set_tracker_mode(tracker_mode_enum tm) { m_tracker_mode = tm; }

  choke_queue*        up_queue()   { return &m_up_queue; }
  choke_queue*        down_queue() { return &m_down_queue; }

  const choke_queue*  c_up_queue() const   { return &m_up_queue; }
  const choke_queue*  c_down_queue() const { return &m_down_queue; }

  uint32_t            up_requested() const   { return std::min(m_up_queue.size_total(), m_up_queue.max_unchoked()); }
  uint32_t            down_requested() const { return std::min(m_down_queue.size_total(), m_down_queue.max_unchoked()); }

  bool                empty() const { return m_first == m_last; }
  uint32_t            size() const { return std::distance(m_first, m_last); }

  uint64_t            up_rate() const;
  uint64_t            down_rate() const;

  unsigned int        up_unchoked() const   { return m_up_queue.size_unchoked(); }
  unsigned int        down_unchoked() const { return m_down_queue.size_unchoked(); }

  // Internal:

  resource_manager_entry* first() { return m_first; }
  resource_manager_entry* last()  { return m_last; }

  void                    set_first(resource_manager_entry* first) { m_first = first; }
  void                    set_last(resource_manager_entry* last)   { m_last = last; }

  void                    inc_iterators() { m_first++; m_last++; }
  void                    dec_iterators() { m_first--; m_last--; }

private:
  std::string             m_name;
  tracker_mode_enum       m_tracker_mode;

  choke_queue             m_up_queue;
  choke_queue             m_down_queue;

  resource_manager_entry* m_first;
  resource_manager_entry* m_last;
};

}

#endif
