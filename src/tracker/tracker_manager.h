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

#ifndef LIBTORRENT_TRACKER_TRACKER_MANAGER_H
#define LIBTORRENT_TRACKER_TRACKER_MANAGER_H

#include <list>
#include <rak/functional.h>

#include <rak/socket_address.h>
#include "globals.h"

namespace torrent {

class AddressList;
class DownloadWrapper;
class TrackerList;
class DownloadInfo;
class Tracker;

class TrackerManager {
public:
  typedef uint32_t                                size_type;
  typedef Tracker*                                value_type;

  typedef rak::mem_fun1<DownloadWrapper, void, AddressList*>       SlotSuccess;
  typedef rak::mem_fun1<DownloadWrapper, void, const std::string&> SlotFailed;

  TrackerManager();
  ~TrackerManager();

  bool                is_active() const                         { return m_active; }
  void                set_active(bool a)                        { m_active = a; }

  bool                is_busy() const;

  void                close();

  void                send_start();
  void                send_stop();
  void                send_completed();
  void                send_later();

  // Request more peers from current, or the next tracker on the
  // list. These functions will start from the current focus and
  // iterate to the next if it is unable to connect. Once the end is
  // reached it will stop.
  bool                request_current();
  void                request_next();

  void                manual_request(bool force);

  void                cycle_group(int group);
  void                randomize();

  size_type           size() const;
  size_type           group_size() const;

  value_type          get(size_type idx) const;
  size_type           focus_index() const;

  void                insert(int group, const std::string& url);

  DownloadInfo*       info();
  const DownloadInfo* info() const;
  void                set_info(DownloadInfo* info);

  TrackerList*        container()                               { return m_control; }

  rak::timer          get_next_timeout() const                  { return m_taskTimeout.time(); }

  void                slot_success(SlotSuccess s)               { m_slotSuccess = s; }
  void                slot_failed(SlotFailed s)                 { m_slotFailed = s; }

  void                receive_success(AddressList* l);
  void                receive_failed(const std::string& msg);

private:
  TrackerManager(const TrackerManager&);
  void operator = (const TrackerManager&);

  void                receive_timeout();

  TrackerList*        m_control;

  bool                m_active;
  bool                m_isRequesting;

  uint32_t            m_numRequests;
  uint32_t            m_maxRequests;
  uint32_t            m_failedRequests;

  uint32_t            m_initialTracker;
  
  SlotSuccess         m_slotSuccess;
  SlotFailed          m_slotFailed;

  rak::priority_item  m_taskTimeout;
};

}

#endif
