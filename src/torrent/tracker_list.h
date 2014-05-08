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

#ifndef LIBTORRENT_TRACKER_LIST_H
#define LIBTORRENT_TRACKER_LIST_H

#include <algorithm>
#include <string>
#include <vector>
#include <torrent/common.h>
#include lt_tr1_functional

namespace torrent {

class AddressList;
class DownloadInfo;
class DownloadWrapper;
class Tracker;

// The tracker list will contain a list of tracker, divided into
// subgroups. Each group must be randomized before we start. When
// starting the tracker request, always start from the beginning and
// iterate if the request failed. Upon request success move the
// tracker to the beginning of the subgroup and start from the
// beginning of the whole list.

class LIBTORRENT_EXPORT TrackerList : private std::vector<Tracker*> {
public:
  friend class DownloadWrapper;

  typedef std::vector<Tracker*> base_type;
  typedef AddressList           address_list;

  typedef std::function<void (Tracker*)>                     slot_tracker;
  typedef std::function<void (Tracker*, const std::string&)> slot_string;
  typedef std::function<uint32_t (Tracker*, AddressList*)>   slot_address_list;

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
  using base_type::front;
  using base_type::back;

  using base_type::at;
  using base_type::operator[];

  TrackerList();

  bool                has_active() const;
  bool                has_active_not_scrape() const;
  bool                has_active_in_group(uint32_t group) const;
  bool                has_active_not_scrape_in_group(uint32_t group) const;
  bool                has_usable() const;

  unsigned int        count_active() const;
  unsigned int        count_usable() const;

  void                close_all() { close_all_excluding(0); }
  void                close_all_excluding(int event_bitmap);

  void                disown_all_including(int event_bitmap);

  void                clear();
  void                clear_stats();

  iterator            insert(unsigned int group, Tracker* tracker);
  void                insert_url(unsigned int group, const std::string& url, bool extra_tracker = false);

  void                send_state(Tracker* tracker, int new_event);
  void                send_state_idx(unsigned idx, int new_event);
  void                send_state_itr(iterator itr, int new_event);

  void                send_scrape(Tracker* tracker);

  DownloadInfo*       info()                                  { return m_info; }
  int                 state()                                 { return m_state; }

  uint32_t            key() const                             { return m_key; }
  void                set_key(uint32_t key)                   { m_key = key; }

  int32_t             numwant() const                         { return m_numwant; }
  void                set_numwant(int32_t n)                  { m_numwant = n; }

  iterator            find(Tracker* tb)                       { return std::find(begin(), end(), tb); }
  iterator            find_url(const std::string& url);

  iterator            find_usable(iterator itr);
  const_iterator      find_usable(const_iterator itr) const;

  iterator            find_next_to_request(iterator itr);

  iterator            begin_group(unsigned int group);
  const_iterator      begin_group(unsigned int group) const;
  iterator            end_group(unsigned int group)           { return begin_group(group + 1); }
  const_iterator      end_group(unsigned int group) const     { return begin_group(group + 1); }

  size_type           size_group() const;
  void                cycle_group(unsigned int group);

  iterator            promote(iterator itr);
  void                randomize_group_entries();

  void                receive_success(Tracker* tb, AddressList* l);
  void                receive_failed(Tracker* tb, const std::string& msg);

  void                receive_scrape_success(Tracker* tb);
  void                receive_scrape_failed(Tracker* tb, const std::string& msg);

  // Used by libtorrent internally.
  slot_address_list&  slot_success()                          { return m_slot_success; }
  slot_string&        slot_failure()                          { return m_slot_failed; }

  slot_tracker&       slot_scrape_success()                   { return m_slot_scrape_success; }
  slot_string&        slot_scrape_failure()                   { return m_slot_scrape_failed; }

  slot_tracker&       slot_tracker_enabled()                  { return m_slot_tracker_enabled; }
  slot_tracker&       slot_tracker_disabled()                 { return m_slot_tracker_disabled; }

protected:
  void                set_info(DownloadInfo* info)            { m_info = info; }

  void                set_state(int s)                        { m_state = s; }

private:
  TrackerList(const TrackerList&) LIBTORRENT_NO_EXPORT;
  void operator = (const TrackerList&) LIBTORRENT_NO_EXPORT;

  DownloadInfo*       m_info;
  int                 m_state;

  uint32_t            m_key;
  int32_t             m_numwant;

  slot_address_list   m_slot_success;
  slot_string         m_slot_failed;

  slot_tracker        m_slot_scrape_success;
  slot_string         m_slot_scrape_failed;

  slot_tracker        m_slot_tracker_enabled;
  slot_tracker        m_slot_tracker_disabled;
};

inline void
TrackerList::send_state_idx(unsigned idx, int new_event) {
  send_state(at(idx), new_event);
}

inline void
TrackerList::send_state_itr(iterator itr, int new_event) {
  if (itr == end())
    return;
    
  send_state(*itr, new_event);
}

}

#endif
