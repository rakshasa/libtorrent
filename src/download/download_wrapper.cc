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
    stop();

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
DownloadWrapper::hash_resume_load() {
  if (!m_main.is_open() || m_main.is_active() || m_hash->is_checked())
    throw client_error("DownloadWrapper::resume_load() called with wrong state");

  if (!m_bencode->has_key_map("libtorrent resume"))
    return;

  try {
    Object& resume  = m_bencode->get_key("libtorrent resume");

    // Load peer addresses.
    if (resume.has_key_string("peers"))
      insert_available_list(resume.get_key("peers").as_string());

    Object& files = resume.get_key("files");

    if (resume.has_key_string("bitfield") &&
        resume.get_key("bitfield").as_string().size() == m_main.content()->bitfield()->size_bytes() &&
        files.as_list().size() == m_main.content()->entry_list()->files_size()) {

      // Clear the hash checking ranges, and add the files ranges we
      // must check.
      m_hash->ranges().clear();
      m_main.content()->bitfield()->from_c_str(resume.get_key("bitfield").as_string().c_str());

    } else {
      // No resume bitfield available, check the whole range.
      m_hash->ranges().insert(0, m_main.content()->chunk_total());
    }

    Object::list_type::iterator bItr = files.as_list().begin();
    EntryList::iterator sItr = m_main.content()->entry_list()->begin();

    // Check the validity of each file, add to the m_hash's ranges if invalid.
    while (sItr != m_main.content()->entry_list()->end()) {
      rak::file_stat fs;

      // Check that the size and modified stamp matches. If not, then
      // add to the hashes to check.

      if (!fs.update((*sItr)->file_meta()->get_path()) ||
          (*sItr)->size() != fs.size() ||
          !bItr->has_key_value("mtime") ||
          bItr->get_key("mtime").as_value() != fs.modified_time())
        m_hash->ranges().insert((*sItr)->range().first, (*sItr)->range().second);

      // Update the priority from the fast resume data.
      if (bItr->has_key_value("priority") &&
          bItr->get_key("priority").as_value() >= 0 &&
          bItr->get_key("priority").as_value() <= PRIORITY_HIGH)
        (*sItr)->set_priority((priority_t)(*bItr).get_key("priority").as_value());

      ++sItr;
      ++bItr;
    }  

  } catch (bencode_error e) {
    m_hash->ranges().insert(0, m_main.content()->chunk_total());
  }

  // Clear bits in invalid regions which will be checked by m_hash.
  //
  // If this is ever optimized in such a way as not to update the
  // bitfield's set size, make sure that Content::update_done() calls
  // Bitfield::update().
  for (HashTorrent::Ranges::iterator itr = m_hash->ranges().begin(); itr != m_hash->ranges().end(); ++itr)
    m_main.content()->bitfield()->unset_range(itr->first, itr->second);

  m_main.content()->update_done();
}

// Break this function up into several smaller functions to make it
// easier to read.
void
DownloadWrapper::hash_resume_save() {
  if (!m_main.is_open() || m_main.is_active())
    throw client_error("DownloadWrapper::resume_save() called with wrong state");

  if (!m_hash->is_checked())
    // We don't remove the old hash data since it might still be
    // valid, just that the client didn't finish the check this time.
    return;

  // If we can't sync, don't save the resume data.
  if (m_main.chunk_list()->sync_all(MemoryChunk::sync_sync) != 0 && m_bencode->has_key("libtorrent resume")) {
    m_bencode->get_key("libtorrent resume").erase_key("bitfield");
    return;
  }

  // Clear the resume data since if the syncing fails we propably don't
  // want the old resume data.
  Object& resume = m_bencode->insert_key("libtorrent resume", Object(Object::TYPE_MAP));

  resume.insert_key("bitfield", std::string((char*)m_main.content()->bitfield()->begin(), m_main.content()->bitfield()->size_bytes()));

  Object::list_type& l = resume.insert_key("files", Object(Object::TYPE_LIST)).as_list();

  EntryList::iterator sItr = m_main.content()->entry_list()->begin();
  
  // Check the validity of each file, add to the m_hash's ranges if invalid.
  while (sItr != m_main.content()->entry_list()->end()) {
    Object& b = *l.insert(l.end(), Object(Object::TYPE_MAP));

    rak::file_stat fs;

    if (!fs.update((*sItr)->file_meta()->get_path())) {
      l.clear();
      break;
    }

    b.insert_key("mtime", fs.modified_time());
    b.insert_key("priority", (int)(*sItr)->priority());

    ++sItr;
  }

  // Save the available peer list. Since this function is called when
  // the download is stopped, we know that all the previously
  // connected peers have been copied to the available list.
  //
  // Consider whetever we need to add currently connected too, as we
  // might save the session while still active.
  extract_available_list(&resume.insert_key("peers", Object()));
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
  m_hash->get_queue()->remove(info()->hash());

  // This could/should be async as we do not care that much if it
  // succeeds or not, any chunks not included in that last
  // hash_resume_save get ignored anyway.
  m_main.chunk_list()->sync_all(MemoryChunk::sync_async);

  m_main.close();

  // Should this perhaps be in stop?
  priority_queue_erase(&taskScheduler, &m_delayDownloadDone);
}

void
DownloadWrapper::start() {
  if (!m_hash->is_checked())
    throw client_error("Tried to start an unchecked download");

  if (!m_main.is_open())
    throw client_error("Tried to start a closed download");

  if (m_main.is_active())
    return;

  m_connectionChunkPassed = signal_chunk_passed().connect(sigc::mem_fun(m_main.delegator()->transfer_list(), &TransferList::index_done));
  m_connectionChunkFailed = signal_chunk_failed().connect(sigc::mem_fun(m_main.delegator()->transfer_list(), &TransferList::index_retry));

  m_main.start();
}

// Remember to update receive_tick() when changing stop().
void
DownloadWrapper::stop() {
  m_main.stop();

  m_connectionChunkPassed.disconnect();
  m_connectionChunkFailed.disconnect();

  m_main.tracker_manager()->send_stop();
}

bool
DownloadWrapper::is_stopped() const {
  return !m_main.tracker_manager()->is_active();
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
DownloadWrapper::extract_available_list(Object* dest) {
  *dest = std::string();
  
  std::string& peers = dest->as_string();
  peers.reserve(m_main.available_list()->size() * sizeof(SocketAddressCompact));
  
  for (AvailableList::const_iterator itr = m_main.available_list()->begin(), last = m_main.available_list()->end(); itr != last; ++itr) {
    if (itr->family() == rak::socket_address::af_inet) {
      SocketAddressCompact sac(itr->sa_inet()->address_n(), itr->sa_inet()->port_n());

      peers.append(sac.c_str(), sizeof(SocketAddressCompact));
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
    m_hash->get_queue()->remove(info()->hash());
    m_main.content()->clear();

  } else if (!m_main.content()->entry_list()->resize_all()) {
    // We couldn't resize the files, tell the client.
    receive_storage_error("Could not resize files in the torrent.");

    // Do we clear the hash?
  }

  if (m_hash->get_queue()->has(info()->hash()))
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
  m_hash->get_queue()->push_back(handle, rak::make_mem_fun(this, &DownloadWrapper::receive_hash_done), info()->hash());
}

void
DownloadWrapper::receive_hash_done(ChunkHandle handle, std::string h) {
  if (!handle.is_valid())
    throw internal_error("DownloadWrapper::receive_hash_done(...) called on an invalid chunk.");

  if (!is_open())
    throw internal_error("DownloadWrapper::receive_hash_done(...) called but the download is not open.");

  uint32_t index = handle->index();
  m_main.chunk_list()->release(handle);

  if (m_hash->is_checking()) {
    m_main.content()->receive_chunk_hash(index, h);
    m_hash->receive_chunkdone();

  } else if (m_hash->is_checked()) {

    if (m_main.chunk_selector()->bitfield()->get(index))
      throw internal_error("DownloadWrapper::receive_hash_done(...) received a chunk that isn't set in ChunkSelector.");

    if (m_main.content()->receive_chunk_hash(index, h)) {

      // Should this be done here, or after send_finished_chunk?
      signal_chunk_passed().emit(index);
      m_main.update_endgame();

      if (m_main.content()->is_done())
        finished_download();
    
      m_main.connection_list()->send_finished_chunk(index);

    } else {
      signal_chunk_failed().emit(index);
    }

  } else {
    // When the HashQueue gets cleared, it will trigger this signal to
    // do cleanup. As HashTorrent must be cleared before the
    // HashQueue, we need to ignore the chunks that get triggered.

    //throw internal_error("DownloadWrapper::receive_hash_done(...) called but we're not in the right state.");
  }
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
