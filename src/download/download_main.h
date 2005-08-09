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

#ifndef LIBTORRENT_DOWNLOAD_MAIN_H
#define LIBTORRENT_DOWNLOAD_MAIN_H

#include "settings.h"
#include "download_state.h"
#include "download_net.h"

#include "protocol/peer_info.h"
#include "utils/task.h"
#include "tracker/tracker_control.h"
#include "tracker/tracker_manager.h"

#include <sigc++/connection.h>

namespace torrent {

class TrackerControl;

class DownloadMain {
public:
  DownloadMain();
  ~DownloadMain();

  void                open();
  void                close();

  void                start();
  void                stop();

  bool                is_open() const                          { return m_state.get_content().is_open(); }
  bool                is_active() const                        { return m_started; }
  bool                is_checked() const                       { return m_checked; }
  bool                is_stopped() const                       { return !m_tracker.is_active(); }

  const std::string&  get_name() const                         { return m_name; }
  void                set_name(const std::string& s)           { m_name = s; }

//   const std::string&  get_hash() const                         { return m_hash; }
//   void                set_hash(const std::string& s)           { m_hash = s; }

  DownloadState&      get_state()                              { return m_state; }
  DownloadNet&        get_net()                                { return m_net; }
  TrackerManager&     get_tracker()                            { return m_tracker; }
  TrackerInfo*        get_info()                               { return m_tracker.tracker_control()->get_info(); }

  // Carefull with these.
  void                setup_delegator();
  void                setup_net();
  void                setup_tracker();

  void                receive_initial_hash();

private:
  // Disable copy ctor and assignment.
  DownloadMain(const DownloadMain&);
  void operator = (const DownloadMain&);

  void                setup_start();
  void                setup_stop();

  void                choke_cycle();

  void                receive_tracker_success();
  void                receive_tracker_request();

  DownloadState       m_state;
  DownloadNet         m_net;
  DownloadSettings    m_settings;
  TrackerManager      m_tracker;

  std::string         m_name;

  bool                m_checked;
  bool                m_started;
  uint32_t            m_lastConnectedSize;

  sigc::connection    m_connectionChunkPassed;
  sigc::connection    m_connectionChunkFailed;
  sigc::connection    m_connectionAddAvailablePeers;

  TaskItem            m_taskChokeCycle;
  TaskItem            m_taskTrackerRequest;
};

} // namespace torrent

#endif // LIBTORRENT_DOWNLOAD_H
