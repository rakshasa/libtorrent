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

#include "config.h"

#include <cstring>
#include <limits>

#include "data/chunk_list.h"
#include "protocol/handshake_manager.h"
#include "protocol/peer_connection_base.h"
#include "tracker/tracker_manager.h"
#include "torrent/exceptions.h"

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

  m_connectionList = new ConnectionList(m_info);
  m_chokeManager = new ChokeManager(m_connectionList);

  m_delegator.slot_chunk_find(rak::make_mem_fun(m_chunkSelector, &ChunkSelector::find));
  m_delegator.slot_chunk_size(rak::make_mem_fun(&m_content, &Content::chunk_index_size));

  m_delegator.transfer_list()->slot_canceled(std::bind1st(std::mem_fun(&ChunkSelector::not_using_index), m_chunkSelector));
  m_delegator.transfer_list()->slot_queued(std::bind1st(std::mem_fun(&ChunkSelector::using_index), m_chunkSelector));
  m_delegator.transfer_list()->slot_completed(std::bind1st(std::mem_fun(&DownloadMain::receive_chunk_done), this));

  m_taskTrackerRequest.set_slot(rak::mem_fn(this, &DownloadMain::receive_tracker_request));

  m_chunkList->slot_create_chunk(rak::make_mem_fun(&m_content, &Content::create_chunk));
  m_chunkList->slot_free_diskspace(rak::make_mem_fun(&m_content, &Content::free_diskspace));
}

DownloadMain::~DownloadMain() {
  if (m_taskTrackerRequest.is_queued())
    throw internal_error("DownloadMain::~DownloadMain(): m_taskTrackerRequest is queued.");

  delete m_trackerManager;
  delete m_chokeManager;
  delete m_connectionList;

  delete m_chunkStatistics;
  delete m_chunkList;
  delete m_chunkSelector;
  delete m_info;
}

void
DownloadMain::open() {
  if (info()->is_open())
    throw internal_error("Tried to open a download that is already open");

  m_content.entry_list()->open();

  m_chunkList->resize(m_content.chunk_total());
  m_chunkStatistics->initialize(m_content.chunk_total());

  info()->set_open(true);
}

void
DownloadMain::close() {
  if (info()->is_active())
    throw internal_error("Tried to close an active download");

  if (!info()->is_open())
    return;

  info()->set_open(false);

  m_trackerManager->close();
  m_delegator.transfer_list()->clear();

  m_content.bitfield()->unallocate();
  m_content.entry_list()->close();

  // Clear the chunklist last as it requires all referenced chunks to
  // be released.
  m_chunkStatistics->clear();
  m_chunkList->clear();
  m_chunkSelector->cleanup();

  m_peerList.clear();
}

void DownloadMain::start() {
  if (!info()->is_open())
    throw client_error("Tried to start a closed download");

  if (info()->is_active())
    throw client_error("Tried to start an active download");

  info()->set_active(true);
  m_lastConnectedSize = 0;

  m_delegator.set_aggressive(false);
  update_endgame();  

  receive_connect_peers();
}  

// struct peer_connection_socket_address : public std::unary_function<const PeerConnectionBase*, const rak::socket_address&> {
//   const rak::socket_address& operator () (const PeerConnectionBase* p) const {
//     return *rak::socket_address::cast_from(p->peer_info()->socket_address());
//   }
// };

void
DownloadMain::stop() {
  if (!info()->is_active())
    return;

  // Set this early so functions like receive_connect_peers() knows
  // not to eat available peers.
  info()->set_active(false);

  m_slotStopHandshakes(this);

  // Save the addresses we are connected to, so we don't need to
  // perform alot of requests to the tracker when restarting the
  // torrent. Consider saving these in the torrent file when dumping
  // it.
//   std::list<rak::socket_address> addressList;
//   std::transform(connection_list()->begin(), connection_list()->end(), std::back_inserter(addressList), peer_connection_socket_address());

//   addressList.sort();
//   available_list()->insert(&addressList);

  while (!connection_list()->empty())
    connection_list()->erase(connection_list()->front());

  priority_queue_erase(&taskScheduler, &m_taskTrackerRequest);
}

uint64_t
DownloadMain::get_bytes_left() const {
  uint64_t left = m_content.entry_list()->bytes_size() - m_content.bytes_completed();

  if (left > ((uint64_t)1 << 60))
    throw internal_error("DownloadMain::get_bytes_left() is too large"); 

  if (m_content.chunks_completed() == m_content.chunk_total() && left != 0)
    throw internal_error("DownloadMain::get_bytes_left() has an invalid size"); 

  return left;
}

void
DownloadMain::update_endgame() {
  if (!m_delegator.get_aggressive() && m_content.chunks_completed() + m_delegator.transfer_list()->size() + 5 >= m_content.chunk_total())
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
DownloadMain::receive_connect_peers() {
  if (!info()->is_active())
    return;

  while (!peer_list()->available_list()->empty() &&
         connection_list()->size() < connection_list()->get_min_size() &&
         connection_list()->size() + m_slotCountHandshakes(this) < connection_list()->get_max_size()) {
    rak::socket_address sa = peer_list()->available_list()->pop_random();

    if (connection_list()->find(sa) == connection_list()->end())
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
  if (connection_list()->size() >= connection_list()->get_min_size())
    return;

  if (connection_list()->size() < m_lastConnectedSize + 10 ||
      !m_trackerManager->request_current())
    // Try the next tracker if we couldn't get enough peers from the
    // current one, or if we have connected more than
    // TrackerManager::max_num_request times.
    m_trackerManager->request_next();

  m_lastConnectedSize = connection_list()->size();
}

}
