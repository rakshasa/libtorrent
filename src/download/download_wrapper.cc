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

#include <iterator>
#include <stdlib.h>
#include <rak/file_stat.h>
#include <sigc++/bind.h>

#include "data/chunk_list.h"
#include "data/content.h"
#include "data/hash_queue.h"
#include "data/hash_torrent.h"
#include "data/file_manager.h"
#include "data/file_meta.h"
#include "protocol/handshake_manager.h"
#include "protocol/peer_connection_base.h"
#include "torrent/exceptions.h"
#include "torrent/object.h"
#include "tracker/tracker_manager.h"

#include "chunk_selector.h"

#include "download_wrapper.h"

namespace torrent {

DownloadWrapper::DownloadWrapper() :
  m_bencode(NULL),
  m_hash(NULL),
  m_connectionType(0) {

  m_delayDownloadDone.set_slot(rak::mem_fn(&m_signalDownloadDone, &Signal::operator()));

  m_main.tracker_manager()->set_info(info());
  m_main.tracker_manager()->slot_success(rak::make_mem_fun(this, &DownloadWrapper::receive_tracker_success));
  m_main.tracker_manager()->slot_failed(rak::make_mem_fun(this, &DownloadWrapper::receive_tracker_failed));
}

DownloadWrapper::~DownloadWrapper() {
  if (m_main.is_active())
    m_main.stop();

  if (m_main.is_open())
    close();

  delete m_hash;
  delete m_bencode;
}

void
DownloadWrapper::initialize(const std::string& hash, const std::string& id) {
  m_main.slot_hash_check_add(rak::make_mem_fun(this, &DownloadWrapper::check_chunk_hash));

  info()->set_hash(hash);
  info()->set_local_id(id);

  info()->slot_stat_left() = rak::make_mem_fun(&m_main, &DownloadMain::get_bytes_left);

  m_main.chunk_list()->set_max_queue_size((32 << 20) / m_main.content()->chunk_size());

  m_main.connection_list()->slot_connected(rak::make_mem_fun(this, &DownloadWrapper::receive_peer_connected));
  m_main.connection_list()->slot_disconnected(rak::make_mem_fun(this, &DownloadWrapper::receive_peer_disconnected));

  // Info hash must be calculate from here on.
  m_hash = new HashTorrent(m_main.chunk_list());

  // Connect various signals and slots.
  m_hash->slot_check_chunk(rak::make_mem_fun(this, &DownloadWrapper::check_chunk_hash));
  m_hash->slot_initial_hash(rak::make_mem_fun(this, &DownloadWrapper::receive_initial_hash));
  m_hash->slot_storage_error(rak::make_mem_fun(this, &DownloadWrapper::receive_storage_error));
}

void
DownloadWrapper::open() {
  if (m_main.is_open())
    return;

  m_main.open();
  m_hash->ranges().insert(0, m_main.content()->chunk_total());
}

void
DownloadWrapper::close() {
  // Stop the hashing first as we need to make sure all chunks are
  // released when DownloadMain::close() is called.
  m_hash->clear();

  // Clear after m_hash to ensure that the empty hash done signal does
  // not get passed to HashTorrent.
  m_hash->get_queue()->remove(this);

  // This could/should be async as we do not care that much if it
  // succeeds or not, any chunks not included in that last
  // hash_resume_save get ignored anyway.
  m_main.chunk_list()->sync_all(MemoryChunk::sync_async);

  m_main.close();

  // Should this perhaps be in stop?
  priority_queue_erase(&taskScheduler, &m_delayDownloadDone);
}

bool
DownloadWrapper::is_stopped() const {
  return !m_main.tracker_manager()->is_active() && !m_main.tracker_manager()->is_busy();
}

void
DownloadWrapper::insert_available_list(const std::string& src) {
  std::list<rak::socket_address> l;

  std::copy(reinterpret_cast<const SocketAddressCompact*>(src.c_str()),
            reinterpret_cast<const SocketAddressCompact*>(src.c_str() + src.size() - src.size() % sizeof(SocketAddressCompact)),
            std::back_inserter(l));
  l.sort();

  m_main.available_list()->insert(&l);
}

void
DownloadWrapper::extract_available_list(std::string& dest) {
  dest.reserve(m_main.available_list()->size() * sizeof(SocketAddressCompact));
  
  for (AvailableList::const_iterator itr = m_main.available_list()->begin(), last = m_main.available_list()->end(); itr != last; ++itr) {
    if (itr->family() == rak::socket_address::af_inet) {
      SocketAddressCompact sac(itr->sa_inet()->address_n(), itr->sa_inet()->port_n());

      dest.append(sac.c_str(), sizeof(SocketAddressCompact));
    }
  }  
}

void
DownloadWrapper::receive_keepalive() {
  for (ConnectionList::iterator itr = m_main.connection_list()->begin(); itr != m_main.connection_list()->end(); )
    if (!(*itr)->receive_keepalive())
      itr = m_main.connection_list()->erase(itr);
    else
      itr++;
}

void
DownloadWrapper::receive_initial_hash() {
  if (m_main.is_active())
    throw internal_error("DownloadWrapper::receive_initial_hash() but we're in a bad state.");

  if (!m_hash->is_checked()) {
    m_hash->clear();

    // Clear after m_hash to ensure that the empty hash done signal does
    // not get passed to HashTorrent.
    m_hash->get_queue()->remove(this);
    m_main.content()->bitfield()->unset_all();

  } else if (!m_main.content()->entry_list()->resize_all()) {
    // We couldn't resize the files, tell the client.
    receive_storage_error("Could not resize files in the torrent.");

    // Do we clear the hash?
  }

  if (m_hash->get_queue()->has(this))
    throw internal_error("DownloadWrapper::receive_initial_hash() found a chunk in the HashQueue.");

  // Initialize the ChunkSelector here so that no chunks will be
  // marked by HashTorrent that are not accounted for.
  m_main.chunk_selector()->initialize(m_main.content()->bitfield(), m_main.chunk_statistics());
  receive_update_priorities();

  m_signalInitialHash.emit();
}    

void
DownloadWrapper::check_chunk_hash(ChunkHandle handle) {
  // Using HashTorrent's queue temporarily.
  m_hash->get_queue()->push_back(handle, rak::make_mem_fun(this, &DownloadWrapper::receive_hash_done));
}

void
DownloadWrapper::receive_hash_done(ChunkHandle handle, const char* hash) {
  if (!handle.is_valid())
    throw internal_error("DownloadWrapper::receive_hash_done(...) called on an invalid chunk.");

  if (!is_open())
    throw internal_error("DownloadWrapper::receive_hash_done(...) called but the download is not open.");

  if (hash == NULL) {
    // Clearing the hash queue, do nothing. Add checks here?

  } else if (m_hash->is_checking()) {
    m_main.content()->receive_chunk_hash(handle.index(), hash);
    m_hash->receive_chunkdone();

  } else if (m_hash->is_checked()) {
    // Receiving chunk hashes after stopping the torrent should be
    // safe.

    if (m_main.chunk_selector()->bitfield()->get(handle.index()))
      throw internal_error("DownloadWrapper::receive_hash_done(...) received a chunk that isn't set in ChunkSelector.");

    if (m_main.content()->receive_chunk_hash(handle.index(), hash)) {

      m_main.delegator()->transfer_list()->hash_succeded(handle.index());
      m_main.update_endgame();

      if (m_main.content()->is_done())
        finished_download();
    
      m_main.connection_list()->send_finished_chunk(handle.index());
      signal_chunk_passed().emit(handle.index());

    } else {
      // This needs to ensure the chunk is still valid.
      m_main.delegator()->transfer_list()->hash_failed(handle.index(), handle.chunk());
      signal_chunk_failed().emit(handle.index());
    }

  } else {
    // When clearing, we get a NULL string, so if none of the above
    // caught this hash, then we got a problem.

    throw internal_error("DownloadWrapper::receive_hash_done(...) Was not expecting non-NULL hash.");
  }

  m_main.chunk_list()->release(&handle);
}  

void
DownloadWrapper::receive_storage_error(const std::string& str) {
  info()->signal_storage_error().emit(str);
}

void
DownloadWrapper::receive_tracker_success(AddressList* l) {
  m_main.connection_list()->set_difference(l);
  m_main.available_list()->insert(l);
  m_main.receive_connect_peers();
  m_main.receive_tracker_success();

  m_signalTrackerSuccess.emit();
}

void
DownloadWrapper::receive_tracker_failed(const std::string& msg) {
  m_signalTrackerFailed.emit(msg);
}

void
DownloadWrapper::receive_peer_connected(PeerConnectionBase* peer) {
  m_signalPeerConnected.emit(Peer(peer));
}

void
DownloadWrapper::receive_peer_disconnected(PeerConnectionBase* peer) {
  m_main.receive_connect_peers();

  m_signalPeerDisconnected.emit(Peer(peer));
}

void
DownloadWrapper::receive_tick() {
  if (!m_main.is_open())
    return;

  unsigned int syncFailed = m_main.chunk_list()->sync_periodic();

  if (m_main.is_active() && syncFailed != 0) {
    // Need to move this stuff into a seperate function.
    m_main.stop();

    m_connectionChunkPassed.disconnect();
    m_connectionChunkFailed.disconnect();

    info()->signal_storage_error().emit("Could not sync data to disk, possibly full.");
  }
}

void
DownloadWrapper::receive_update_priorities() {
  if (m_main.chunk_selector()->empty())
    return;

  m_main.chunk_selector()->high_priority()->clear();
  m_main.chunk_selector()->normal_priority()->clear();

  for (EntryList::iterator itr = m_main.content()->entry_list()->begin(); itr != m_main.content()->entry_list()->end(); ++itr) {
    if ((*itr)->priority() == 1)
      m_main.chunk_selector()->normal_priority()->insert((*itr)->range().first, (*itr)->range().second);

    else if ((*itr)->priority() == 2)
      m_main.chunk_selector()->high_priority()->insert((*itr)->range().first, (*itr)->range().second);
  }

  m_main.chunk_selector()->update_priorities();

  std::for_each(m_main.connection_list()->begin(), m_main.connection_list()->end(), std::mem_fun(&PeerConnectionBase::update_interested));
}

void
DownloadWrapper::finished_download() {
  // We delay emitting the signal to allow the delegator to
  // clean up. If we do a straight call it would cause problems
  // for clients that wish to close and reopen the torrent, as
  // HashQueue, Delegator etc shouldn't be cleaned up at this
  // point.
  //
  // This needs to be seperated into a new function.
  if (!m_delayDownloadDone.is_queued())
    priority_queue_insert(&taskScheduler, &m_delayDownloadDone, cachedTime);

  m_main.connection_list()->erase_seeders();
  info()->down_rate()->reset_rate();
}

}
