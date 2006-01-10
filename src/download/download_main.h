// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#include <rak/functional.h>
#include <sigc++/signal.h>

#include "available_list.h"
#include "connection_list.h"
#include "delegator.h"

#include "data/content.h"
#include "data/chunk_handle.h"
#include "protocol/peer_info.h"
#include "globals.h"
#include "torrent/rate.h"

namespace torrent {

class ChunkList;
class ChokeManager;
class ChunkSelector;
class DownloadWrapper;
class HandshakeManager;
class TrackerManager;
class ThrottleList;

class DownloadMain {
public:
  DownloadMain();
  ~DownloadMain();

  void                open();
  void                close();

  void                start();
  void                stop();

  bool                is_open() const                            { return m_isOpen; }
  bool                is_active() const                          { return m_started; }

  ChokeManager*       choke_manager()                            { return m_chokeManager; }
  TrackerManager*     tracker_manager()                          { return m_trackerManager; }
  TrackerManager*     tracker_manager() const                    { return m_trackerManager; }

  // Only retrive writable chunks when the download is active.
  ChunkList*          chunk_list()                               { return m_chunkList; }
  ChunkSelector*      chunk_selector()                           { return m_chunkSelector; }
  Content*            content()                                  { return &m_content; }
  Delegator*          delegator()                                { return &m_delegator; }

  AvailableList*      available_list()                           { return &m_availableList; }
  ConnectionList*     connection_list()                          { return &m_connectionList; }

  ThrottleList*       upload_throttle()                          { return m_uploadThrottle; }
  void                set_upload_throttle(ThrottleList* t)       { m_uploadThrottle = t; }

  ThrottleList*       download_throttle()                        { return m_downloadThrottle; }
  void                set_download_throttle(ThrottleList* t)     { m_downloadThrottle = t; }

  bool                get_endgame() const                        { return m_endgame; }
  uint64_t            get_bytes_left() const;

  Rate*               up_rate()                                  { return &m_upRate; }
  Rate*               down_rate()                                { return &m_downRate; }

  // Carefull with these.
  void                setup_delegator();
  void                setup_tracker();

  typedef sigc::signal1<void, const std::string&>                SignalString;
  typedef sigc::signal1<void, uint32_t>                          SignalChunk;

  typedef rak::mem_fun3<HandshakeManager, void, const SocketAddress&,
		       const std::string&, const std::string&>         SlotStartHandshake;
  typedef rak::mem_fun1<HandshakeManager, uint32_t, const std::string&> SlotCountHandshakes;
  typedef rak::mem_fun1<DownloadWrapper, void, ChunkHandle>             SlotHashCheckAdd;

  SignalString&       signal_network_log()                       { return m_signalNetworkLog; }
  SignalString&       signal_storage_error()                     { return m_signalStorageError; }

  void                slot_start_handshake(SlotStartHandshake s)   { m_slotStartHandshake = s; }
  void                slot_count_handshakes(SlotCountHandshakes s) { m_slotCountHandshakes = s; }
  void                slot_hash_check_add(SlotHashCheckAdd s)      { m_slotHashCheckAdd = s; }

  void                receive_connect_peers();
  void                receive_chunk_done(unsigned int index);

  void                receive_tracker_success();
  void                receive_tracker_request();

  void                update_endgame();

private:
  // Disable copy ctor and assignment.
  DownloadMain(const DownloadMain&);
  void operator = (const DownloadMain&);

  void                setup_start();
  void                setup_stop();

  TrackerManager*     m_trackerManager;
  ChokeManager*       m_chokeManager;

  ChunkList*          m_chunkList;
  ChunkSelector*      m_chunkSelector;
  Content             m_content;
  Delegator           m_delegator;

  AvailableList       m_availableList;
  ConnectionList      m_connectionList;

  bool                m_started;
  bool                m_isOpen;
  bool                m_endgame;
  uint32_t            m_lastConnectedSize;

  ThrottleList*       m_uploadThrottle;
  ThrottleList*       m_downloadThrottle;

  Rate                m_upRate;
  Rate                m_downRate;

  SignalString        m_signalNetworkLog;
  SignalString        m_signalStorageError;

  SlotStartHandshake  m_slotStartHandshake;
  SlotCountHandshakes m_slotCountHandshakes;
  SlotHashCheckAdd    m_slotHashCheckAdd;

  rak::priority_item  m_taskTrackerRequest;
};

}

#endif
