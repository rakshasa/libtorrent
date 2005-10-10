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

#include "config.h"

#include <cstring>
#include <limits>
#include <sigc++/signal.h>

#include "data/chunk_list.h"
#include "parse/parse.h"
#include "protocol/handshake_manager.h"
#include "protocol/peer_connection_base.h"
#include "torrent/exceptions.h"
#include "tracker/tracker_manager.h"

#include "choke_manager.h"
#include "delegator_select.h"
#include "download_main.h"

namespace torrent {

DownloadMain::DownloadMain() :
  m_trackerManager(new TrackerManager()),
  m_chokeManager(new ChokeManager(&this->m_connectionList)),
  m_chunkList(new ChunkList),

  m_started(false),
  m_isOpen(false),
  m_endgame(false),

  m_upRate(60),
  m_downRate(60) {

  m_content.block_download_done(true);

  m_taskTick.set_slot(sigc::mem_fun(*this, &DownloadMain::receive_tick));
  m_taskTick.set_iterator(taskScheduler.end());

  m_taskTrackerRequest.set_slot(sigc::mem_fun(*this, &DownloadMain::receive_tracker_request));
  m_taskTrackerRequest.set_iterator(taskScheduler.end());

  m_chunkList->slot_create_chunk(rak::make_mem_fn(&m_content, &Content::create_chunk));
}

DownloadMain::~DownloadMain() {
  if (taskScheduler.is_scheduled(&m_taskTick) ||
      taskScheduler.is_scheduled(&m_taskTrackerRequest))
    throw internal_error("DownloadMain::~DownloadMain(): m_taskTick is scheduled");

  delete m_trackerManager;
  delete m_chokeManager;
  delete m_chunkList;
}

void
DownloadMain::open() {
  if (is_open())
    throw internal_error("Tried to open a download that is already open");

  m_content.open();
  m_chunkList->resize(m_content.chunk_total());
  m_bitfieldCounter.create(m_content.chunk_total());

//   m_delegator.get_select().get_priority().add(Priority::NORMAL, 0, m_content.chunk_total());

  m_isOpen = true;
}

void
DownloadMain::close() {
  if (is_active())
    throw internal_error("Tried to close an active download");

  if (!is_open())
    return;

  m_isOpen = false;

  m_trackerManager->close();
  m_delegator.clear();
  m_content.close();

  // Clear the chunklist last as it requires all referenced chunks to
  // be released.
  m_chunkList->clear();
}

void DownloadMain::start() {
  if (!is_open())
    throw client_error("Tried to start a closed download");

  if (is_active())
    return;

  m_started = true;
  m_lastConnectedSize = 0;

  setup_start();
  m_trackerManager->send_start();

  receive_connect_peers();
}  

void
DownloadMain::stop() {
  if (!m_started)
    return;

  // Set this early so functions like receive_connect_peers() knows
  // not to eat available peers.
  m_started = false;

  // Save the addresses we are connected to, so we don't need to
  // perform alot of requests to the tracker when restarting the
  // torrent. Consider saving these in the torrent file when dumping
  // it.
  std::list<SocketAddress> addressList;

  std::transform(connection_list()->begin(), connection_list()->end(), std::back_inserter(addressList),
		 rak::on(std::mem_fun(&PeerConnectionBase::get_peer), std::mem_fun_ref<const torrent::SocketAddress&>(&PeerInfo::get_socket_address)));

  addressList.sort();
  available_list()->insert(&addressList);

  while (!connection_list()->empty())
    connection_list()->erase(connection_list()->front());

  m_trackerManager->send_stop();
  setup_stop();

  m_chunkList->sync_all();
}

uint64_t
DownloadMain::get_bytes_left() const {
  uint64_t left = m_content.entry_list()->get_bytes_size() - m_content.get_bytes_completed();

  if (left > ((uint64_t)1 << 60))
    throw internal_error("DownloadMain::get_bytes_left() is too large"); 

  if (m_content.get_chunks_completed() == m_content.chunk_total() && left != 0)
    throw internal_error("DownloadMain::get_bytes_left() has an invalid size"); 

  return left;
}

void
DownloadMain::update_endgame() {
  if (!m_endgame &&
      m_content.get_chunks_completed() + m_delegator.get_chunks().size() + 0 >= m_content.chunk_total()) {
    m_endgame = true;
    m_delegator.set_aggressive(true);
  }
}

void
DownloadMain::receive_tick() {
  taskScheduler.insert(&m_taskTick, (Timer::cache() + 30 * 1000000).round_seconds());
  // Now done by the resource manager.
  //m_chokeManager->cycle();

  m_chunkList->sync_periodic();
}

void
DownloadMain::receive_chunk_done(unsigned int index) {
  ChunkHandle handle = m_chunkList->get(index, false);

  if (!handle.is_valid())
    throw storage_error("DownloadState::chunk_done(...) called with an index we couldn't retrieve from storage");

  m_slotHashCheckAdd(handle);
}

void
DownloadMain::receive_hash_done(ChunkHandle handle, std::string h) {
  if (!handle.is_valid())
    throw internal_error("DownloadMain::receive_hash_done(...) called on an invalid chunk.");

  if (h.empty() || !is_open()) {
    // Ignore.

  } else if (std::memcmp(h.c_str(), m_content.get_hash_c(handle->index()), 20) != 0) {
    m_signalChunkFailed.emit(handle->index());

  } else {
    m_content.mark_done(handle->index());
    m_signalChunkPassed.emit(handle->index());

    update_endgame();

    if (m_content.is_done())
      m_connectionList.erase_seeders();
    
    m_connectionList.send_finished_chunk(handle->index());
  }

  m_chunkList->release(handle);
}  

void
DownloadMain::receive_connect_peers() {
  if (!m_started)
    return;

  while (!available_list()->empty() &&
	 connection_list()->size() < connection_list()->get_min_size() &&
	 connection_list()->size() + m_slotCountHandshakes() < connection_list()->get_max_size()) {
    SocketAddress sa = available_list()->pop_random();

    if (connection_list()->find(sa) == connection_list()->end())
      m_slotStartHandshake(sa);
  }
}

void
DownloadMain::receive_tracker_success() {
  if (!m_started)
    return;

  taskScheduler.erase(&m_taskTrackerRequest);
  taskScheduler.insert(&m_taskTrackerRequest, (Timer::cache() + 30 * 1000000).round_seconds());
}

void
DownloadMain::receive_tracker_request() {
  if (connection_list()->size() >= connection_list()->get_min_size())
    return;

  if (connection_list()->size() >= m_lastConnectedSize + 10)
    m_trackerManager->request_current();
  else // Check to make sure we don't query after every connection to the primary tracker?
    m_trackerManager->request_next();

  m_lastConnectedSize = connection_list()->size();
}

}
