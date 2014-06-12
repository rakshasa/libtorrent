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

#include "config.h"

#include <cstring>
#include <limits>

#include "data/chunk_list.h"
#include "protocol/extensions.h"
#include "protocol/handshake_manager.h"
#include "protocol/initial_seed.h"
#include "protocol/peer_connection_base.h"
#include "protocol/peer_factory.h"
#include "torrent/download.h"
#include "torrent/exceptions.h"
#include "torrent/throttle.h"
#include "torrent/data/file_list.h"
#include "torrent/download/download_manager.h"
#include "torrent/download/choke_queue.h"
#include "torrent/download/choke_group.h"
#include "torrent/peer/connection_list.h"
#include "torrent/peer/peer.h"
#include "torrent/peer/peer_info.h"
#include "torrent/tracker_controller.h"
#include "torrent/tracker_list.h"
#include "torrent/utils/log.h"

#include "available_list.h"
#include "chunk_selector.h"
#include "chunk_statistics.h"
#include "download_main.h"
#include "download_wrapper.h"

#define LT_LOG_THIS(log_level, log_fmt, ...)                         \
  lt_log_print_info(LOG_TORRENT_##log_level, m_ptr->info(), "download", log_fmt, __VA_ARGS__);

namespace tr1 { using namespace std::tr1; }

namespace torrent {

DownloadInfo::DownloadInfo() :
  m_flags(flag_compact | flag_accepting_new_peers | flag_accepting_seeders | flag_pex_enabled | flag_pex_active),

  m_upRate(60),
  m_downRate(60),
  m_skipRate(60),
  
  m_uploadedBaseline(0),
  m_completedBaseline(0),
  m_sizePex(0),
  m_maxSizePex(8),
  m_metadataSize(0),
  
  m_creationDate(0),
  m_loadDate(rak::timer::current_seconds()),

  m_upload_unchoked(0),
  m_download_unchoked(0) {
}

DownloadMain::DownloadMain() :
  m_info(new DownloadInfo),

  m_choke_group(NULL),
  m_chunkList(new ChunkList),
  m_chunkSelector(new ChunkSelector(file_list()->mutable_data())),
  m_chunkStatistics(new ChunkStatistics),

  m_initialSeeding(NULL),
  m_uploadThrottle(NULL),
  m_downloadThrottle(NULL) {

  m_tracker_list = new TrackerList();
  m_tracker_controller = new TrackerController(m_tracker_list);

  m_tracker_list->slot_success() = tr1::bind(&TrackerController::receive_success, m_tracker_controller, tr1::placeholders::_1, tr1::placeholders::_2);
  m_tracker_list->slot_failure() = tr1::bind(&TrackerController::receive_failure, m_tracker_controller, tr1::placeholders::_1, tr1::placeholders::_2);
  m_tracker_list->slot_scrape_success() = tr1::bind(&TrackerController::receive_scrape, m_tracker_controller, tr1::placeholders::_1);
  m_tracker_list->slot_tracker_enabled()  = tr1::bind(&TrackerController::receive_tracker_enabled, m_tracker_controller, tr1::placeholders::_1);
  m_tracker_list->slot_tracker_disabled() = tr1::bind(&TrackerController::receive_tracker_disabled, m_tracker_controller, tr1::placeholders::_1);

  m_connectionList = new ConnectionList(this);

  m_delegator.slot_chunk_find() = std::tr1::bind(&ChunkSelector::find, m_chunkSelector, tr1::placeholders::_1, tr1::placeholders::_2);
  m_delegator.slot_chunk_size() = std::tr1::bind(&FileList::chunk_index_size, file_list(), tr1::placeholders::_1);

  m_delegator.transfer_list()->slot_canceled()  = std::tr1::bind(&ChunkSelector::not_using_index, m_chunkSelector, tr1::placeholders::_1);
  m_delegator.transfer_list()->slot_queued()    = std::tr1::bind(&ChunkSelector::using_index, m_chunkSelector, tr1::placeholders::_1);
  m_delegator.transfer_list()->slot_completed() = std::tr1::bind(&DownloadMain::receive_chunk_done, this, tr1::placeholders::_1);
  m_delegator.transfer_list()->slot_corrupt()   = std::tr1::bind(&DownloadMain::receive_corrupt_chunk, this, tr1::placeholders::_1);

  m_delayDisconnectPeers.slot() = std::tr1::bind(&ConnectionList::disconnect_queued, m_connectionList);
  m_taskTrackerRequest.slot() = std::tr1::bind(&DownloadMain::receive_tracker_request, this);

  m_chunkList->set_data(file_list()->mutable_data());
  m_chunkList->slot_create_chunk() = tr1::bind(&FileList::create_chunk_index, file_list(), tr1::placeholders::_1, tr1::placeholders::_2);
  m_chunkList->slot_free_diskspace() = tr1::bind(&FileList::free_diskspace, file_list());
}

DownloadMain::~DownloadMain() {
  if (m_taskTrackerRequest.is_queued())
    throw internal_error("DownloadMain::~DownloadMain(): m_taskTrackerRequest is queued.");

  // Check if needed.
  m_connectionList->clear();
  m_tracker_list->clear();

  if (m_info->size_pex() != 0)
    throw internal_error("DownloadMain::~DownloadMain(): m_info->size_pex() != 0.");

  delete m_tracker_controller;
  delete m_tracker_list;
  delete m_connectionList;

  delete m_chunkStatistics;
  delete m_chunkList;
  delete m_chunkSelector;
  delete m_info;

  m_ut_pex_delta.clear();
  m_ut_pex_initial.clear();
}

std::pair<ThrottleList*, ThrottleList*>
DownloadMain::throttles(const sockaddr* sa) {
  ThrottlePair pair = ThrottlePair(NULL, NULL);

  if (manager->connection_manager()->address_throttle())
    pair = manager->connection_manager()->address_throttle()(sa);

  return std::make_pair(pair.first == NULL ? upload_throttle() : pair.first->throttle_list(),
                        pair.second == NULL ? download_throttle() : pair.second->throttle_list());
}

void
DownloadMain::open(int flags) {
  if (info()->is_open())
    throw internal_error("Tried to open a download that is already open");

  file_list()->open(flags & FileList::open_no_create);

  m_chunkList->resize(file_list()->size_chunks());
  m_chunkStatistics->initialize(file_list()->size_chunks());

  info()->set_flags(DownloadInfo::flag_open);
}

void
DownloadMain::close() {
  if (info()->is_active())
    throw internal_error("Tried to close an active download");

  if (!info()->is_open())
    return;

  info()->unset_flags(DownloadInfo::flag_open);

  // Don't close the tracker manager here else it will cause STOPPED
  // requests to be lost. TODO: Check that this is valid.
//   m_trackerManager->close();

  m_delegator.transfer_list()->clear();

  file_list()->close();

  // Clear the chunklist last as it requires all referenced chunks to
  // be released.
  m_chunkStatistics->clear();
  m_chunkList->clear();
  m_chunkSelector->cleanup();
}

void DownloadMain::start() {
  if (!info()->is_open())
    throw internal_error("Tried to start a closed download");

  if (info()->is_active())
    throw internal_error("Tried to start an active download");

  info()->set_flags(DownloadInfo::flag_active);
  chunk_list()->set_flags(ChunkList::flag_active);

  m_delegator.set_aggressive(false);
  update_endgame();  

  receive_connect_peers();
}  

void
DownloadMain::stop() {
  if (!info()->is_active())
    return;

  // Set this early so functions like receive_connect_peers() knows
  // not to eat available peers.
  info()->unset_flags(DownloadInfo::flag_active);
  chunk_list()->unset_flags(ChunkList::flag_active);

  m_slotStopHandshakes(this);
  connection_list()->erase_remaining(connection_list()->begin(), ConnectionList::disconnect_available);

  delete m_initialSeeding;
  m_initialSeeding = NULL;

  priority_queue_erase(&taskScheduler, &m_delayDisconnectPeers);
  priority_queue_erase(&taskScheduler, &m_taskTrackerRequest);

  if (info()->upload_unchoked() != 0 || info()->download_unchoked() != 0)
    throw internal_error("DownloadMain::stop(): info()->upload_unchoked() != 0 || info()->download_unchoked() != 0.");
}

bool
DownloadMain::start_initial_seeding() {
  if (!file_list()->is_done())
    return false;

  m_initialSeeding = new InitialSeeding(this);
  return true;
}

void
DownloadMain::initial_seeding_done(PeerConnectionBase* pcb) {
  if (m_initialSeeding == NULL)
    throw internal_error("DownloadMain::initial_seeding_done called when not initial seeding.");

  // Close all connections but the currently active one (pcb), that
  // one will be closed by throw close_connection() later.
  //
  // When calling initial_seed()->new_peer(...) the 'pcb' won't be in
  // the connection list, so don't treat it as an error. Make sure to
  // catch close_connection() at the caller of new_peer(...) and just
  // close the filedesc before proceeding as normal.
  ConnectionList::iterator pcb_itr = std::find(m_connectionList->begin(), m_connectionList->end(), pcb);

  if (pcb_itr != m_connectionList->end()) {
    std::iter_swap(m_connectionList->begin(), pcb_itr);
    m_connectionList->erase_remaining(m_connectionList->begin() + 1, ConnectionList::disconnect_available);
  } else {
    m_connectionList->erase_remaining(m_connectionList->begin(), ConnectionList::disconnect_available);
  }

  // Switch to normal seeding.
  DownloadManager::iterator itr = manager->download_manager()->find(m_info);
  (*itr)->set_connection_type(Download::CONNECTION_SEED);
  m_connectionList->slot_new_connection(&createPeerConnectionSeed);

  delete m_initialSeeding;
  m_initialSeeding = NULL;

  // And close the current connection.
  throw close_connection();
}

void
DownloadMain::update_endgame() {
  if (!m_delegator.get_aggressive() &&
      file_list()->completed_chunks() + m_delegator.transfer_list()->size() + 5 >= file_list()->size_chunks())
    m_delegator.set_aggressive(true);
}

void
DownloadMain::receive_chunk_done(unsigned int index) {
  ChunkHandle handle = m_chunkList->get(index);

  if (!handle.is_valid())
    throw storage_error("DownloadState::chunk_done(...) called with an index we couldn't retrieve from storage");

  m_slotHashCheckAdd(handle);
}

void
DownloadMain::receive_corrupt_chunk(PeerInfo* peerInfo) {
  peerInfo->set_failed_counter(peerInfo->failed_counter() + 1);

  // Just use some very primitive heuristics here to decide if we're
  // going to disconnect the peer. Also, consider adding a flag so we
  // don't recalculate these things whenever the peer reconnects.

  // That is... non at all ;)

  if (peerInfo->failed_counter() > HandshakeManager::max_failed)
    connection_list()->erase(peerInfo, ConnectionList::disconnect_unwanted);
}

void
DownloadMain::add_peer(const rak::socket_address& sa) {
  m_slotStartHandshake(sa, this);
}

void
DownloadMain::receive_connect_peers() {
  if (!info()->is_active())
    return;

  // TODO: Is this actually going to be used?
  AddressList* alist = peer_list()->available_list()->buffer();

  if (!alist->empty()) {
    alist->sort();
    peer_list()->insert_available(alist);
    alist->clear();
  }

  while (!peer_list()->available_list()->empty() &&
         manager->connection_manager()->can_connect() &&
         connection_list()->size() < connection_list()->min_size() &&
         connection_list()->size() + m_slotCountHandshakes(this) < connection_list()->max_size()) {
    rak::socket_address sa = peer_list()->available_list()->pop_random();

    if (connection_list()->find(sa.c_sockaddr()) == connection_list()->end())
      m_slotStartHandshake(sa, this);
  }
}

void
DownloadMain::receive_tracker_success() {
  if (!info()->is_active())
    return;

  priority_queue_erase(&taskScheduler, &m_taskTrackerRequest);
  priority_queue_insert(&taskScheduler, &m_taskTrackerRequest, (cachedTime + rak::timer::from_seconds(10)).round_seconds());
}

void
DownloadMain::receive_tracker_request() {
  bool should_stop = false;
  bool should_start = false;

  if (info()->is_pex_enabled() && info()->size_pex() > 0)
    should_stop = true;

  if (connection_list()->size() + peer_list()->available_list()->size() / 2 < connection_list()->min_size())
    should_start = true;

  if (should_stop)
    m_tracker_controller->stop_requesting();
  else if (should_start)
    m_tracker_controller->start_requesting();
}

struct SocketAddressCompact_less {
  bool operator () (const SocketAddressCompact& a, const SocketAddressCompact& b) const {
    return (a.addr < b.addr) || ((a.addr == b.addr) && (a.port < b.port));
  }
};

void
DownloadMain::do_peer_exchange() {
  if (!info()->is_active())
    throw internal_error("DownloadMain::do_peer_exchange called on inactive download.");

  // Check whether we should tell the peers to stop/start sending PEX
  // messages.
  int togglePex = 0;

  if (!m_info->is_pex_active() &&
      m_connectionList->size() < m_connectionList->min_size() / 2 &&
      m_peerList.available_list()->size() < m_peerList.available_list()->max_size() / 4) {
    m_info->set_flags(DownloadInfo::flag_pex_active);

    // Only set PEX_ENABLE if we don't have max_size_pex set to zero.
    if (m_info->size_pex() < m_info->max_size_pex())
      togglePex = PeerConnectionBase::PEX_ENABLE;

  } else if (m_info->is_pex_active() &&
             m_connectionList->size() >= m_connectionList->min_size()) {
//              m_peerList.available_list()->size() >= m_peerList.available_list()->max_size() / 2) {
    togglePex = PeerConnectionBase::PEX_DISABLE;
    m_info->unset_flags(DownloadInfo::flag_pex_active);
  }

  // Return if we don't really want to do anything?

  ProtocolExtension::PEXList current;

  for (ConnectionList::iterator itr = m_connectionList->begin(); itr != m_connectionList->end(); ++itr) {
    PeerConnectionBase* pcb = (*itr)->m_ptr();
    const rak::socket_address* sa = rak::socket_address::cast_from(pcb->peer_info()->socket_address());

    if (pcb->peer_info()->listen_port() != 0 && sa->family() == rak::socket_address::af_inet)
      current.push_back(SocketAddressCompact(sa->sa_inet()->address_n(), pcb->peer_info()->listen_port()));

    if (!pcb->extensions()->is_remote_supported(ProtocolExtension::UT_PEX))
      continue;

    if (togglePex == PeerConnectionBase::PEX_ENABLE) {
      pcb->set_peer_exchange(true);

      if (m_info->size_pex() >= m_info->max_size_pex())
        togglePex = 0;

    } else if (!pcb->extensions()->is_local_enabled(ProtocolExtension::UT_PEX)) {
      continue;

    } else if (togglePex == PeerConnectionBase::PEX_DISABLE) {
      pcb->set_peer_exchange(false);

      continue;
    }

    // Still using the old buffer? Make a copy in this rare case.
    DataBuffer* message = pcb->extension_message();

    if (!message->empty() && (message->data() == m_ut_pex_initial.data() || message->data() == m_ut_pex_delta.data())) {
      char* buffer = new char[message->length()];
      memcpy(buffer, message->data(), message->length());
      message->set(buffer, buffer + message->length(), true);
    }

    pcb->do_peer_exchange();
  }

  std::sort(current.begin(), current.end(), SocketAddressCompact_less());

  ProtocolExtension::PEXList added;
  added.reserve(current.size());
  std::set_difference(current.begin(), current.end(), m_ut_pex_list.begin(), m_ut_pex_list.end(), 
                      std::back_inserter(added), SocketAddressCompact_less());

  ProtocolExtension::PEXList removed;
  removed.reserve(m_ut_pex_list.size());
  std::set_difference(m_ut_pex_list.begin(), m_ut_pex_list.end(), current.begin(), current.end(), 
                      std::back_inserter(removed), SocketAddressCompact_less());
    
  if (current.size() > m_info->max_size_pex_list()) {
    // This test is only correct as long as we have a constant max
    // size.
    if (added.size() < current.size() - m_info->max_size_pex_list())
      throw internal_error("DownloadMain::do_peer_exchange() added.size() < current.size() - m_info->max_size_pex_list().");

    // Randomize this:
    added.erase(added.end() - (current.size() - m_info->max_size_pex_list()), added.end());

    // Create the new m_ut_pex_list by removing any 'removed'
    // addresses from the original list and then adding the new
    // addresses.
    m_ut_pex_list.erase(std::set_difference(m_ut_pex_list.begin(), m_ut_pex_list.end(), removed.begin(), removed.end(),
                                            m_ut_pex_list.begin(), SocketAddressCompact_less()), m_ut_pex_list.end());
    m_ut_pex_list.insert(m_ut_pex_list.end(), added.begin(), added.end());

    std::sort(m_ut_pex_list.begin(), m_ut_pex_list.end(), SocketAddressCompact_less());

  } else {
    m_ut_pex_list.swap(current);
  }

  current.clear();
  m_ut_pex_delta.clear();

  // If no peers were added or removed, the initial message is still correct and
  // the delta message stays emptied. Otherwise generate the appropriate messages.
  if (!added.empty() || !m_ut_pex_list.empty()) {
    m_ut_pex_delta = ProtocolExtension::generate_ut_pex_message(added, removed);

    m_ut_pex_initial.clear();
    m_ut_pex_initial = ProtocolExtension::generate_ut_pex_message(m_ut_pex_list, current);
  }
}

void
DownloadMain::set_metadata_size(size_t size) {
  if (m_info->is_meta_download()) {
    if (m_fileList.size_bytes() < 2)
      file_list()->reset_filesize(size);
    else if (size != m_fileList.size_bytes())
      throw communication_error("Peer-supplied metadata size mismatch.");

  } else if (m_info->metadata_size() && m_info->metadata_size() != size) {
      throw communication_error("Peer-supplied metadata size mismatch.");
  }

  m_info->set_metadata_size(size);
}

}
