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

#include "config.h"

#include <cstring>
#include <limits>

#include "data/chunk_list.h"
#include "protocol/extensions.h"
#include "protocol/handshake_manager.h"
#include "protocol/peer_connection_base.h"
#include "tracker/tracker_manager.h"
#include "torrent/exceptions.h"
#include "torrent/data/file_list.h"
#include "torrent/peer/connection_list.h"
#include "torrent/peer/peer.h"
#include "torrent/peer/peer_info.h"

#include "available_list.h"
#include "choke_manager.h"
#include "chunk_selector.h"
#include "chunk_statistics.h"
#include "download_info.h"
#include "download_main.h"

namespace torrent {

DownloadMain::DownloadMain() :
  m_info(new DownloadInfo),

  m_trackerManager(new TrackerManager()),
  m_chunkList(new ChunkList),
  m_chunkSelector(new ChunkSelector),
  m_chunkStatistics(new ChunkStatistics),

  m_uploadThrottle(NULL),
  m_downloadThrottle(NULL) {

  m_connectionList       = new ConnectionList(this);
  m_uploadChokeManager   = new ChokeManager(m_connectionList);
  m_downloadChokeManager = new ChokeManager(m_connectionList, ChokeManager::flag_unchoke_all_new);

  m_uploadChokeManager->set_slot_choke_weight(&calculate_upload_choke);
  m_uploadChokeManager->set_slot_unchoke_weight(&calculate_upload_unchoke);
  m_uploadChokeManager->set_slot_connection(std::mem_fun(&PeerConnectionBase::receive_upload_choke));

  std::memcpy(m_uploadChokeManager->choke_weight(),   weights_upload_choke,   ChokeManager::weight_size_bytes);
  std::memcpy(m_uploadChokeManager->unchoke_weight(), weights_upload_unchoke, ChokeManager::weight_size_bytes);

  m_downloadChokeManager->set_slot_choke_weight(&calculate_download_choke);
  m_downloadChokeManager->set_slot_unchoke_weight(&calculate_download_unchoke);
  m_downloadChokeManager->set_slot_connection(std::mem_fun(&PeerConnectionBase::receive_download_choke));

  std::memcpy(m_downloadChokeManager->choke_weight(),   weights_download_choke,   ChokeManager::weight_size_bytes);
  std::memcpy(m_downloadChokeManager->unchoke_weight(), weights_download_unchoke, ChokeManager::weight_size_bytes);

  m_delegator.slot_chunk_find(rak::make_mem_fun(m_chunkSelector, &ChunkSelector::find));
  m_delegator.slot_chunk_size(rak::make_mem_fun(file_list(), &FileList::chunk_index_size));

  m_delegator.transfer_list()->slot_canceled(std::bind1st(std::mem_fun(&ChunkSelector::not_using_index), m_chunkSelector));
  m_delegator.transfer_list()->slot_queued(std::bind1st(std::mem_fun(&ChunkSelector::using_index), m_chunkSelector));
  m_delegator.transfer_list()->slot_completed(std::bind1st(std::mem_fun(&DownloadMain::receive_chunk_done), this));
  m_delegator.transfer_list()->slot_corrupt(std::bind1st(std::mem_fun(&DownloadMain::receive_corrupt_chunk), this));

  m_taskTrackerRequest.set_slot(rak::mem_fn(this, &DownloadMain::receive_tracker_request));

  m_chunkList->slot_create_chunk(rak::make_mem_fun(file_list(), &FileList::create_chunk_index));
  m_chunkList->slot_free_diskspace(rak::make_mem_fun(file_list(), &FileList::free_diskspace));
}

DownloadMain::~DownloadMain() {
  if (m_taskTrackerRequest.is_queued())
    throw internal_error("DownloadMain::~DownloadMain(): m_taskTrackerRequest is queued.");

  // Check if needed.
  m_connectionList->clear();

  if (m_info->size_pex() != 0)
    throw internal_error("DownloadMain::~DownloadMain(): m_info->size_pex() != 0.");

  delete m_trackerManager;
  delete m_uploadChokeManager;
  delete m_downloadChokeManager;
  delete m_connectionList;

  delete m_chunkStatistics;
  delete m_chunkList;
  delete m_chunkSelector;
  delete m_info;

  m_ut_pex_delta.clear();
  m_ut_pex_initial.clear();
}

void
DownloadMain::open(int flags) {
  if (info()->is_open())
    throw internal_error("Tried to open a download that is already open");

  file_list()->open(flags & FileList::open_no_create);

  m_chunkList->resize(file_list()->size_chunks());
  m_chunkStatistics->initialize(file_list()->size_chunks());

  info()->set_open(true);
}

void
DownloadMain::close() {
  if (info()->is_active())
    throw internal_error("Tried to close an active download");

  if (!info()->is_open())
    return;

  info()->set_open(false);

  // Don't close the tracker manager here else it will cause STOPPED
  // requests to be lost. TODO: Check that this is valid.
//   m_trackerManager->close();

  m_delegator.transfer_list()->clear();

  file_list()->mutable_bitfield()->unallocate();
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

  info()->set_active(true);
  m_lastConnectedSize = 0;

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
  info()->set_active(false);

  m_slotStopHandshakes(this);

  connection_list()->erase_remaining(connection_list()->begin(), ConnectionList::disconnect_available);

  priority_queue_erase(&taskScheduler, &m_taskTrackerRequest);
}

void
DownloadMain::update_endgame() {
  if (!m_delegator.get_aggressive() &&
      file_list()->completed_chunks() + m_delegator.transfer_list()->size() + 5 >= file_list()->size_chunks())
    m_delegator.set_aggressive(true);
}

void
DownloadMain::receive_chunk_done(unsigned int index) {
  ChunkHandle handle = m_chunkList->get(index, false);

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
  connection_list()->erase(peerInfo, ConnectionList::disconnect_unwanted);
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
  priority_queue_insert(&taskScheduler, &m_taskTrackerRequest, (cachedTime + rak::timer::from_seconds(30)).round_seconds());
}

void
DownloadMain::receive_tracker_request() {
  if (connection_list()->size() >= connection_list()->min_size())
    return;

  if (m_info->is_pex_enabled() || connection_list()->size() < m_lastConnectedSize + 10)
    m_trackerManager->request_next();
  else if (!m_trackerManager->request_current())
    m_trackerManager->request_next();

  m_lastConnectedSize = connection_list()->size();
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
    m_info->set_pex_active(true);

    // Only set PEX_ENABLE if we don't have max_size_pex set to zero.
    if (m_info->size_pex() < m_info->max_size_pex())
      togglePex = PeerConnectionBase::PEX_ENABLE;

  } else if (m_info->is_pex_active() &&
             m_connectionList->size() >= m_connectionList->min_size()) {
//              m_peerList.available_list()->size() >= m_peerList.available_list()->max_size() / 2) {
    togglePex = PeerConnectionBase::PEX_DISABLE;
    m_info->set_pex_active(false);
  }

  // Return if we don't really want to do anything?

  ProtocolExtension::PEXList current;

  for (ConnectionList::iterator itr = m_connectionList->begin(); itr != m_connectionList->end(); ++itr) {
    PeerConnectionBase* pcb = (*itr)->ptr();
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

}
