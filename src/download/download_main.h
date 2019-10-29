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

#ifndef LIBTORRENT_DOWNLOAD_MAIN_H
#define LIBTORRENT_DOWNLOAD_MAIN_H

#include <deque>
#include <rak/functional.h>

#include "globals.h"

#include "delegator.h"

#include "data/chunk_handle.h"
#include "download/available_list.h"
#include "net/data_buffer.h"
#include "torrent/download_info.h"
#include "torrent/download/group_entry.h"
#include "torrent/data/file_list.h"
#include "torrent/peer/peer_list.h"

namespace torrent {

class ChunkList;
class ChunkSelector;
class ChunkStatistics;

class choke_group;
class ConnectionList;
class DownloadWrapper;
class HandshakeManager;
class TrackerController;
class TrackerList;
class WebseedController;
class DownloadInfo;
class ThrottleList;
class InitialSeeding;

class DownloadMain {
public:
  typedef std::deque<std::pair<rak::timer, uint32_t> > have_queue_type;
  typedef std::vector<SocketAddressCompact>            pex_list;

  DownloadMain();
  ~DownloadMain();

  void                open(int flags);
  void                close();

  void                start();
  void                stop();

  class choke_group*       choke_group()                              { return m_choke_group; }
  const class choke_group* c_choke_group() const                      { return m_choke_group; }
  void                set_choke_group(class choke_group* grp)         { m_choke_group = grp; }

  TrackerController*  tracker_controller()                       { return m_tracker_controller; }
  TrackerList*        tracker_list()                             { return m_tracker_list; }

  WebseedController*  webseed_controller()                       { return m_webseed_controller; }

  DownloadInfo*       info()                                     { return m_info; }

  // Only retrieve writable chunks when the download is active.
  ChunkList*          chunk_list()                               { return m_chunkList; }
  ChunkSelector*      chunk_selector()                           { return m_chunkSelector; }
  ChunkStatistics*    chunk_statistics()                         { return m_chunkStatistics; }
  
  Delegator*          delegator()                                { return &m_delegator; }

  have_queue_type*    have_queue()                               { return &m_haveQueue; }

  InitialSeeding*     initial_seeding()                          { return m_initialSeeding; }
  bool                start_initial_seeding();
  void                initial_seeding_done(PeerConnectionBase* pcb);

  ConnectionList*     connection_list()                          { return m_connectionList; }
  FileList*           file_list()                                { return &m_fileList; }
  PeerList*           peer_list()                                { return &m_peerList; }

  std::pair<ThrottleList*, ThrottleList*> throttles(const sockaddr* sa);

  ThrottleList*       upload_throttle()                          { return m_uploadThrottle; }
  void                set_upload_throttle(ThrottleList* t)       { m_uploadThrottle = t; }

  ThrottleList*       download_throttle()                        { return m_downloadThrottle; }
  void                set_download_throttle(ThrottleList* t)     { m_downloadThrottle = t; }

  group_entry*        up_group_entry()                           { return &m_up_group_entry; }
  group_entry*        down_group_entry()                         { return &m_down_group_entry; }

  DataBuffer          get_ut_pex(bool initial)                   { return (initial ? m_ut_pex_initial : m_ut_pex_delta).clone(); }

  bool                want_pex_msg()                             { return m_info->is_pex_active() && m_peerList.available_list()->want_more(); }; 

  void                set_metadata_size(size_t s);

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

  void                add_peer(const rak::socket_address& sa);

  void                receive_connect_peers();
  void                receive_chunk_done(unsigned int index);
  void                receive_corrupt_chunk(PeerInfo* peerInfo);

  void                receive_tracker_success();
  void                receive_tracker_request();

  void                receive_do_peer_exchange();

  void                do_peer_exchange();

  void                update_endgame();

  rak::priority_item& delay_download_done()       { return m_delay_download_done; }
  rak::priority_item& delay_partially_done()      { return m_delay_partially_done; }
  rak::priority_item& delay_partially_restarted() { return m_delay_partially_restarted; }

  rak::priority_item& delay_disconnect_peers() { return m_delayDisconnectPeers; }

private:
  // Disable copy ctor and assignment.
  DownloadMain(const DownloadMain&);
  void operator = (const DownloadMain&);

  void                setup_start();
  void                setup_stop();

  DownloadInfo*       m_info;

  TrackerController*  m_tracker_controller;
  TrackerList*        m_tracker_list;

  WebseedController*  m_webseed_controller;

  class choke_group*  m_choke_group;

  group_entry         m_up_group_entry;
  group_entry         m_down_group_entry;

  ChunkList*          m_chunkList;
  ChunkSelector*      m_chunkSelector;
  ChunkStatistics*    m_chunkStatistics;

  Delegator           m_delegator;
  have_queue_type     m_haveQueue;
  InitialSeeding*     m_initialSeeding;

  ConnectionList*     m_connectionList;
  FileList            m_fileList;
  PeerList            m_peerList;

  DataBuffer          m_ut_pex_delta;
  DataBuffer          m_ut_pex_initial;
  pex_list            m_ut_pex_list;

  ThrottleList*       m_uploadThrottle;
  ThrottleList*       m_downloadThrottle;

  slot_start_handshake_type m_slotStartHandshake;
  slot_stop_handshakes_type m_slotStopHandshakes;

  SlotCountHandshakes m_slotCountHandshakes;
  SlotHashCheckAdd    m_slotHashCheckAdd;

  rak::priority_item  m_delay_download_done;
  rak::priority_item  m_delay_partially_done;
  rak::priority_item  m_delay_partially_restarted;

  rak::priority_item  m_delayDisconnectPeers;
  rak::priority_item  m_taskTrackerRequest;
};

}

#endif
