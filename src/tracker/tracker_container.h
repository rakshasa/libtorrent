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

#ifndef LIBTORRENT_TRACKER_TRACKER_CONTAINER_H
#define LIBTORRENT_TRACKER_TRACKER_CONTAINER_H

#include <algorithm>
#include <vector>
#include <rak/functional.h>

namespace torrent {

class AddressList;
class DownloadInfo;
class Tracker;

// The tracker list will contain a list of tracker, divided into
// subgroups. Each group must be randomized before we start. When
// starting the tracker request, always start from the beginning and
// iterate if the request failed. Upon request success move the
// tracker to the beginning of the subgroup and start from the
// beginning of the whole list.

class TrackerContainer : private std::vector<Tracker*> {
public:
  typedef std::vector<Tracker*>                                   base_type;
  typedef rak::mem_fun1<TrackerManager, void, AddressList*>       slot_success_type;
  typedef rak::mem_fun1<TrackerManager, void, const std::string&> slot_failed_type;

  using base_type::value_type;

  using base_type::iterator;
  using base_type::const_iterator;
  using base_type::reverse_iterator;
  using base_type::const_reverse_iterator;
  using base_type::size;
  using base_type::empty;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  using base_type::operator[];

  TrackerContainer();
  ~TrackerContainer() { clear(); }

  bool                has_active() const;
  bool                has_enabled() const;

  void                close_all();
  void                clear();

  iterator            insert(unsigned int group, Tracker* t);

  iterator            promote(iterator itr);

  void                send_state(int s);

  void                randomize();
  void                cycle_group(int group);

  DownloadInfo*       info()                                  { return m_info; }
  void                set_info(DownloadInfo* info)            { m_info = info; }

  int                 state()                                 { return m_state; }
  void                set_state(int s)                        { m_state = s; }

  iterator            find(Tracker* tb)                       { return std::find(begin(), end(), tb); }
  iterator            find_enabled(iterator itr);
  const_iterator      find_enabled(const_iterator itr) const;

  iterator            begin_group(unsigned int group);
  iterator            end_group(unsigned int group)           { return begin_group(group + 1); }
  void                cycle_group(unsigned int group);

  uint32_t            time_last_connection() const            { return m_timeLastConnection; }
  void                set_time_last_connection(uint32_t v)    { m_timeLastConnection = v; }

  // Functions for controlling the current focus. They only support
  // one active tracker atm.
  iterator            focus()                                 { return m_itr; }
  const_iterator      focus() const                           { return m_itr; }
  void                set_focus(iterator itr)                 { m_itr = itr; }

  bool                focus_next_group();

  uint32_t            focus_normal_interval() const;
  uint32_t            focus_min_interval() const;

  void                slot_success(slot_success_type s)       { m_slotSuccess = s; }
  void                slot_failed(slot_failed_type s)         { m_slotFailed = s; }

  void                receive_success(Tracker* tb, AddressList* l);
  void                receive_failed(Tracker* tb, const std::string& msg);

private:
  DownloadInfo*       m_info;
  int                 m_state;

  uint32_t            m_timeLastConnection;

  iterator            m_itr;

  slot_success_type   m_slotSuccess;
  slot_failed_type    m_slotFailed;
};

}

#endif
