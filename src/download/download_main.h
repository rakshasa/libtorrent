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
#include "delegator.h"

#include "data/content.h"
#include "protocol/peer_info.h"
#include "utils/bitfield_counter.h"
#include "utils/task.h"
#include "torrent/rate.h"

namespace torrent {

class ChunkListNode;
class TrackerManager;

class DownloadMain {
public:
  DownloadMain();
  ~DownloadMain();

  void                open();
  void                close();

  void                start();
  void                stop();

  bool                is_open() const                            { return m_content.is_open(); }
  bool                is_active() const                          { return m_started; }
  bool                is_checked() const                         { return m_checked; }

  ChokeManager*       choke_manager()                            { return &m_chokeManager; }
  TrackerManager*     tracker_manager()                          { return m_trackerManager; }
  TrackerManager*     tracker_manager() const                    { return m_trackerManager; }

  Content*            content()                                  { return &m_content; }
  Delegator*          delegator()                                { return &m_delegator; }

  AvailableList*      available_list()                           { return &m_availableList; }
  ConnectionList*     connection_list()                          { return &m_connectionList; }

  bool                get_endgame() const                        { return m_endgame; }
  uint64_t            get_bytes_left();

  Rate&               get_up_rate()                           { return m_upRate; }
  Rate&               get_down_rate()                            { return m_downRate; }

  BitFieldCounter&    get_bitfield_counter()                     { return m_bitfieldCounter; }

  // Carefull with these.
  void                setup_delegator();
  void                setup_net();
  void                setup_tracker();

  void                choke_balance();
  void                choke_cycle();
  int                 size_unchoked();

  typedef sigc::signal1<void, const std::string&>                SignalString;
  typedef sigc::signal1<void, uint32_t>                          SignalChunk;
  typedef sigc::slot1<void, const SocketAddress&>                SlotStartHandshake;
  typedef sigc::slot0<uint32_t>                                  SlotCountHandshakes;
  typedef sigc::slot1<void, ChunkListNode*>                      SlotHashCheckAdd;

  SignalString&       signal_network_log()                       { return m_signalNetworkLog; }
  SignalString&       signal_storage_error()                     { return m_signalStorageError; }

  SignalChunk&        signal_chunk_passed()                      { return m_signalChunkPassed; }
  SignalChunk&        signal_chunk_failed()                      { return m_signalChunkFailed; }

  void                slot_start_handshake(SlotStartHandshake s)   { m_slotStartHandshake = s; }
  void                slot_count_handshakes(SlotCountHandshakes s) { m_slotCountHandshakes = s; }
  void                slot_hash_check_add(SlotHashCheckAdd s)      { m_slotHashCheckAdd = s; }

  void                receive_connect_peers();
  void                receive_initial_hash();

  void                receive_choke_cycle();
  void                receive_chunk_done(unsigned int index);
  void                receive_hash_done(ChunkListNode* node, std::string h);

  void                receive_tracker_success();
  void                receive_tracker_request();

private:
  // Disable copy ctor and assignment.
  DownloadMain(const DownloadMain&);
  void operator = (const DownloadMain&);

  void                setup_start();
  void                setup_stop();

  void                update_endgame();

  TrackerManager*     m_trackerManager;
  ChokeManager        m_chokeManager;

  Content             m_content;
  Delegator           m_delegator;

  AvailableList       m_availableList;
  ConnectionList      m_connectionList;

  bool                m_checked;
  bool                m_started;
  bool                m_endgame;
  uint32_t            m_lastConnectedSize;

  Rate                m_upRate;
  Rate                m_downRate;

  BitFieldCounter     m_bitfieldCounter;

  sigc::connection    m_connectionChunkPassed;
  sigc::connection    m_connectionChunkFailed;
  sigc::connection    m_connectionAddAvailablePeers;

  SignalString        m_signalNetworkLog;
  SignalString        m_signalStorageError;

  SignalChunk         m_signalChunkPassed;
  SignalChunk         m_signalChunkFailed;

  SlotStartHandshake  m_slotStartHandshake;
  SlotCountHandshakes m_slotCountHandshakes;
  SlotHashCheckAdd    m_slotHashCheckAdd;

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
