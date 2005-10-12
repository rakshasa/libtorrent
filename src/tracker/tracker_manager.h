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

#ifndef LIBTORRENT_TRACKER_TRACKER_MANAGER_H
#define LIBTORRENT_TRACKER_TRACKER_MANAGER_H

#include "utils/task.h"

namespace torrent {

class TrackerControl;
class TrackerInfo;
class TrackerBase;

class TrackerManager {
public:
  typedef uint32_t                     size_type;
  typedef std::pair<int, TrackerBase*> value_type;

  TrackerManager();
  ~TrackerManager();

  bool                is_active() const;
  bool                is_busy() const;

  void                close();

  void                send_start();
  void                send_stop();
  void                send_completed();

  // Request more peers from current, or the next tracker on the
  // list. These functions will start from the current focus and
  // iterate to the next if it is unable to connect. Once the end is
  // reached it will stop.
  void                request_current();
  void                request_next();

  void                manual_request(bool force);

  void                cycle_group(int group);
  void                randomize();

  size_type           size() const;

  value_type          get(size_type idx) const;
  size_type           focus_index() const;

  void                insert(int group, const std::string& url);

  TrackerInfo*        tracker_info();

  Timer               get_next_timeout() const;

private:
  TrackerManager(const TrackerManager&);
  void operator = (const TrackerManager&);

  void                receive_timeout();
  void                receive_success();
  void                receive_failed();

  TrackerControl*     m_control;

  bool                m_isRequesting;

  uint32_t            m_numRequests;
  uint32_t            m_maxRequests;

  uint32_t            m_initialTracker;

  TaskItem            m_taskTimeout;
};

inline Timer
TrackerManager::get_next_timeout() const {
  return taskScheduler.is_scheduled(&m_taskTimeout) ? m_taskTimeout.get_time() : Timer();
}

}

#endif
