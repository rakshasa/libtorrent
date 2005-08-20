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

#include <sigc++/connection.h>

#include "available_list.h"
#include "choke_manager.h"
#include "connection_list.h"
#include "download_state.h"

#include "content/delegator.h"
#include "protocol/peer_info.h"
#include "utils/task.h"
#include "torrent/rate.h"

namespace torrent {

class TrackerManager;

class DownloadMain {
public:
  DownloadMain();
  ~DownloadMain();

  void                open();
  void                close();

  void                start();
  void                stop();

  bool                is_open() const                            { return m_state.get_content().is_open(); }
  bool                is_active() const                          { return m_started; }
  bool                is_checked() const                         { return m_checked; }

  DownloadState*      state()                                    { return &m_state; }

  ChokeManager*       choke_manager()                            { return &m_chokeManager; }
  TrackerManager*     tracker_manager()                          { return m_trackerManager; }
  TrackerManager*     tracker_manager() const                    { return m_trackerManager; }

  Delegator*          delegator()                                { return &m_delegator; }

  AvailableList*      available_list()                           { return &m_availableList; }
  ConnectionList*     connection_list()                          { return &m_connectionList; }

  bool                get_endgame() const                        { return m_endgame; }
  void                set_endgame(bool b);

  Rate&               get_write_rate()                           { return m_writeRate; }
  Rate&               get_read_rate()                            { return m_readRate; }

  // Carefull with these.
  void                setup_delegator();
  void                setup_net();
  void                setup_tracker();

  void                choke_balance();
  void                choke_cycle();
  int                 size_unchoked();

  void                receive_connect_peers();
  void                receive_initial_hash();

  typedef sigc::signal1<void, const std::string&>                SignalString;
  typedef sigc::slot1<void, const SocketAddress&>                SlotStartHandshake;
  typedef sigc::slot0<uint32_t>                                  SlotCountHandshakes;

  SignalString&       signal_network_log()                       { return m_signalNetworkLog; }

  void                slot_start_handshake(SlotStartHandshake s)   { m_slotStartHandshake = s; }
  void                slot_count_handshakes(SlotCountHandshakes s) { m_slotCountHandshakes = s; }

private:
  // Disable copy ctor and assignment.
  DownloadMain(const DownloadMain&);
  void operator = (const DownloadMain&);

  void                setup_start();
  void                setup_stop();

  void                receive_choke_cycle();

  void                receive_tracker_success();
  void                receive_tracker_request();

  DownloadState       m_state;

  TrackerManager*     m_trackerManager;
  ChokeManager        m_chokeManager;
  Delegator           m_delegator;

  AvailableList       m_availableList;
  ConnectionList      m_connectionList;

  bool                m_checked;
  bool                m_started;
  bool                m_endgame;
  uint32_t            m_lastConnectedSize;

  Rate                m_writeRate;
  Rate                m_readRate;

  sigc::connection    m_connectionChunkPassed;
  sigc::connection    m_connectionChunkFailed;
  sigc::connection    m_connectionAddAvailablePeers;

  SignalString        m_signalNetworkLog;
  SlotStartHandshake  m_slotStartHandshake;
  SlotCountHandshakes m_slotCountHandshakes;

  TaskItem            m_taskChokeCycle;
  TaskItem            m_taskTrackerRequest;
};

inline void
DownloadMain::choke_balance() {
  m_chokeManager.balance(connection_list()->begin(), connection_list()->end());
}

inline void
DownloadMain::choke_cycle() {
  m_chokeManager.cycle(connection_list()->begin(), connection_list()->end());
}

inline int
DownloadMain::size_unchoked() {
  return m_chokeManager.get_unchoked(connection_list()->begin(), connection_list()->end());
}

}

#endif
