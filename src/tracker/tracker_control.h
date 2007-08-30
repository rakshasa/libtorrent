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

#ifndef LIBTORRENT_TRACKER_TRACKER_CONTROL_H
#define LIBTORRENT_TRACKER_TRACKER_CONTROL_H

#include <list>
#include <string>
#include <rak/functional.h>
#include <rak/socket_address.h>

#include "download/download_info.h"

#include "tracker_base.h"
#include "tracker_container.h"

#include "globals.h"

namespace torrent {

// We change to NONE once we successfully send. If STOPPED then don't
// retry and remove from service. We only send the state to the first
// tracker we can connect to, NONE's should be interpreted as a
// started download.

class AddressList;
class TrackerManager;

class TrackerControl {
public:
  typedef rak::mem_fun1<TrackerManager, void, AddressList*>       SlotSuccess;
  typedef rak::mem_fun1<TrackerManager, void, const std::string&> SlotFailed;

  TrackerControl();

  bool                is_busy() const;

  void                send_state(DownloadInfo::State s);
  inline void         close();

  void                insert(int group, const std::string& url);
  void                cycle_group(int group);

  DownloadInfo::State get_state()                             { return m_state; }
  void                set_state(DownloadInfo::State s)        { m_state = s; }

  DownloadInfo*       info()                                  { return m_info; }
  void                set_info(DownloadInfo* info)            { m_info = info; }

  TrackerContainer&   get_list()                              { return m_list; }

  uint32_t            focus_index() const                     { return m_itr - m_list.begin(); }
  void                set_focus_index(uint32_t v);

  uint32_t            end_index() const                       { return m_list.size(); }

  bool                focus_next_group();

  rak::timer          time_last_connection() const            { return m_timeLastConnection; }

  uint32_t            focus_normal_interval() const;
  uint32_t            focus_min_interval() const;

  void                slot_success(SlotSuccess s)             { m_slotSuccess = s; }
  void                slot_failed(SlotFailed s)               { m_slotFailed = s; }

private:

  TrackerControl(const TrackerControl& t);
  void                operator = (const TrackerControl& t);

  // Rename to receive_addresses or something?
  void                receive_success(TrackerBase* tb, AddressList* l);
  void                receive_failed(TrackerBase* tb, const std::string& msg);

  int                 m_tries;

  DownloadInfo*       m_info;
  DownloadInfo::State m_state;

  TrackerContainer           m_list;
  TrackerContainer::iterator m_itr;

  rak::timer          m_timeLastConnection;

  SlotSuccess         m_slotSuccess;
  SlotFailed          m_slotFailed;
};

inline bool
TrackerControl::is_busy() const {
  return m_itr != m_list.end() && m_itr->second->is_busy();
}

inline void
TrackerControl::close() {
  if (m_itr != m_list.end())
    m_itr->second->close();
}

}

#endif
