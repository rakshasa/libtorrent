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

#ifndef LIBTORRENT_DOWNLOAD_MAIN_H
#define LIBTORRENT_DOWNLOAD_MAIN_H

#include <deque>
#include <rak/functional.h>
#include <sigc++/signal.h>

#include "globals.h"

#include "download_info.h"
#include "delegator.h"

#include "data/chunk_handle.h"
#include "download/available_list.h"
#include "net/data_buffer.h"
#include "torrent/data/file_list.h"
#include "torrent/peer/peer_list.h"

namespace torrent {

class ChunkList;
class ChunkSelector;
class ChunkStatistics;

class ChokeManager;
class ConnectionList;
class DownloadWrapper;
class HandshakeManager;
class TrackerManager;
class DownloadInfo;
class ThrottleList;

class DownloadMain {
public:
  typedef std::deque<std::pair<rak::timer, uint32_t> > have_queue_type;
  typedef std::vector<SocketAddressCompact>            pex_list;

  DownloadMain();
  ~DownloadMain();

  void                open();
  void                close();

  void                start();
  void                stop();

  ChokeManager*       upload_choke_manager()                     { return m_uploadChokeManager; }
  ChokeManager*       download_choke_manager()                   { return m_downloadChokeManager; }
  TrackerManager*     tracker_manager() const                    { return m_trackerManager; }

  DownloadInfo*       info()                                     { return m_info; }

  // Only retrieve writable chunks when the download is active.
  ChunkList*          chunk_list()                               { return m_chunkList; }
  ChunkSelector*      chunk_selector()                           { return m_chunkSelector; }
  ChunkStatistics*    chunk_statistics()                         { return m_chunkStatistics; }
  
  Delegator*          delegator()                                { return &m_delegator; }

  have_queue_type*    have_queue()                               { return &m_haveQueue; }

  ConnectionList*     connection_list()                          { return m_connectionList; }
  FileList*           file_list()                                { return &m_fileList; }
  PeerList*           peer_list()                                { return &m_peerList; }

  ThrottleList*       upload_throttle()                          { return m_uploadThrottle; }
  void                set_upload_throttle(ThrottleList* t)       { m_uploadThrottle = t; }

  ThrottleList*       download_throttle()                        { return m_downloadThrottle; }
  void                set_download_throttle(ThrottleList* t)     { m_downloadThrottle = t; }

  DataBuffer          get_ut_pex(bool initial)                   { return initial ? m_ut_pex_initial : m_ut_pex_delta; }

  bool                want_pex_msg()                             { return m_info->is_pex_active() && m_peerList.available_list()->want_more(); }; 

  // Carefull with these.
  void                setup_delegator();
  void                setup_tracker();

  typedef rak::const_mem_fun1<HandshakeManager, uint32_t, DownloadMain*>                   SlotCountHandshakes;
  typedef rak::mem_fun1<DownloadWrapper, void, ChunkHandle>                                SlotHashCheckAdd;

  typedef rak::mem_fun2<HandshakeManager, void, const rak::socket_address&, DownloadMain*> slot_start_handshake_type;
  typedef rak::mem_fun1<HandshakeManager, void, DownloadMain*>                             slot_stop_handshakes_type;

  void                slot_start_handshake(slot_start_handshake_type s) { m_slotStartHandshake = s; }
  void                slot_stop_handshakes(slot_stop_handshakes_type s) { m_slotStopHandshakes = s; }
  void                slot_count_handshakes(SlotCountHandshakes s) { m_slotCountHandshakes = s; }
  void                slot_hash_check_add(SlotHashCheckAdd s)      { m_slotHashCheckAdd = s; }

  void                receive_connect_peers();
  void                receive_chunk_done(unsigned int index);
  void                receive_corrupt_chunk(PeerInfo* peerInfo);

  void                receive_tracker_success();
  void                receive_tracker_request();

  void                receive_do_peer_exchange();

  void                do_peer_exchange();

  void                update_endgame();

private:
  // Disable copy ctor and assignment.
  DownloadMain(const DownloadMain&);
  void operator = (const DownloadMain&);

  void                setup_start();
  void                setup_stop();

  DownloadInfo*       m_info;

  TrackerManager*     m_trackerManager;
  ChokeManager*       m_uploadChokeManager;
  ChokeManager*       m_downloadChokeManager;

  ChunkList*          m_chunkList;
  ChunkSelector*      m_chunkSelector;
  ChunkStatistics*    m_chunkStatistics;

  Delegator           m_delegator;
  have_queue_type     m_haveQueue;

  ConnectionList*     m_connectionList;
  FileList            m_fileList;
  PeerList            m_peerList;

  uint32_t            m_lastConnectedSize;

  DataBuffer          m_ut_pex_delta;
  DataBuffer          m_ut_pex_initial;
  pex_list            m_ut_pex_list;

  ThrottleList*       m_uploadThrottle;
  ThrottleList*       m_downloadThrottle;

  slot_start_handshake_type m_slotStartHandshake;
  slot_stop_handshakes_type m_slotStopHandshakes;

  SlotCountHandshakes m_slotCountHandshakes;
  SlotHashCheckAdd    m_slotHashCheckAdd;

  rak::priority_item  m_taskTrackerRequest;
};

}

#endif
