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

#include <stdlib.h>
#include <sigc++/bind.h>

#include "data/chunk_list.h"
#include "data/content.h"
#include "data/hash_queue.h"
#include "data/hash_torrent.h"
#include "data/file_manager.h"
#include "data/file_meta.h"
#include "data/file_stat.h"
#include "protocol/handshake_manager.h"
#include "protocol/peer_connection_base.h"
#include "torrent/exceptions.h"
#include "tracker/tracker_manager.h"

#include "download_wrapper.h"

namespace torrent {

DownloadWrapper::DownloadWrapper() :
  m_hash(NULL),

  m_connectionType(0) {
}

DownloadWrapper::~DownloadWrapper() {
  if (m_main.is_active())
    stop();

  if (m_main.is_open())
    close();

  delete m_hash;
}

void
DownloadWrapper::initialize(const std::string& hash, const std::string& id, const SocketAddress& sa) {
  m_main.setup_delegator();
  m_main.setup_tracker();

  m_main.slot_hash_check_add(rak::make_mem_fn(this, &DownloadWrapper::check_chunk_hash));

  m_main.tracker_manager()->tracker_info()->set_hash(hash);
  m_main.tracker_manager()->tracker_info()->set_local_id(id);
  m_main.tracker_manager()->tracker_info()->set_local_address(sa);
  m_main.tracker_manager()->tracker_info()->set_key(random());
  m_main.tracker_manager()->slot_success(rak::make_mem_fn(this, &DownloadWrapper::receive_tracker_success));
  m_main.tracker_manager()->slot_failed(rak::make_mem_fn(this, &DownloadWrapper::receive_tracker_failed));

  m_main.chunk_list()->set_max_queue_size((32 << 20) / m_main.content()->chunk_size());

  m_main.connection_list()->slot_connected(rak::make_mem_fn(this, &DownloadWrapper::receive_peer_connected));
  m_main.connection_list()->slot_disconnected(rak::make_mem_fn(this, &DownloadWrapper::receive_peer_disconnected));

  // Info hash must be calculate from here on.
  m_hash = new HashTorrent(m_main.chunk_list());

  // Connect various signals and slots.
  m_hash->slot_check_chunk(rak::make_mem_fn(this, &DownloadWrapper::check_chunk_hash));
  m_hash->slot_initial_hash(rak::make_mem_fn(this, &DownloadWrapper::receive_initial_hash));
  m_hash->slot_storage_error(rak::make_mem_fn(this, &DownloadWrapper::receive_storage_error));
}

void
DownloadWrapper::hash_resume_load() {
  if (!m_main.is_open() || m_main.is_active() || m_hash->is_checked())
    throw client_error("DownloadWrapper::resume_load() called with wrong state");

  if (!m_bencode.has_key("libtorrent resume"))
    return;

  try {
    Bencode& resume  = m_bencode.get_key("libtorrent resume");

    // Load peer addresses.
    if (resume.has_key("peers") && resume.get_key("peers").is_string()) {
      const std::string& peers = resume.get_key("peers").as_string();

      std::list<SocketAddress> l(reinterpret_cast<const SocketAddressCompact*>(peers.c_str()),
				 reinterpret_cast<const SocketAddressCompact*>(peers.c_str() + peers.size() - peers.size() % sizeof(SocketAddressCompact)));

      l.sort();
      m_main.available_list()->insert(&l);
    }

    Bencode& files = resume.get_key("files");

    if (resume.get_key("bitfield").as_string().size() != m_main.content()->bitfield().size_bytes() ||
	files.as_list().size() != m_main.content()->entry_list()->files_size())
      return;

    // Clear the hash checking ranges, and add the files ranges we must check.
    m_hash->ranges().clear();

    std::memcpy(m_main.content()->bitfield().begin(), resume.get_key("bitfield").as_string().c_str(), m_main.content()->bitfield().size_bytes());

    Bencode::List::iterator bItr = files.as_list().begin();
    EntryList::iterator sItr = m_main.content()->entry_list()->begin();

    // Check the validity of each file, add to the m_hash's ranges if invalid.
    while (sItr != m_main.content()->entry_list()->end()) {
      FileStat fs;

      // Check that the size and modified stamp matches. If not, then
      // add to the hashes to check.

      if (fs.update(sItr->file_meta()->get_path()) ||
	  sItr->size() != fs.size() ||
	  !bItr->has_key("mtime") ||
	  !(*bItr).get_key("mtime").is_value() ||
	  (*bItr).get_key("mtime").as_value() != fs.get_mtime())
	m_hash->ranges().insert(sItr->range().first, sItr->range().second);

      // Update the priority from the fast resume data.
      if (bItr->has_key("priority") &&
	  (*bItr).get_key("priority").is_value() &&
	  (*bItr).get_key("priority").as_value() >= 0 &&
	  (*bItr).get_key("priority").as_value() < 3)
	sItr->set_priority((*bItr).get_key("priority").as_value());

      ++sItr;
      ++bItr;
    }  

  } catch (bencode_error e) {
    m_hash->ranges().insert(0, m_main.content()->chunk_total());
  }

  // Clear bits in invalid regions which will be checked by m_hash.
  for (Priority::Ranges::iterator itr = m_hash->ranges().begin(); itr != m_hash->ranges().end(); ++itr)
    m_main.content()->bitfield().set(itr->first, itr->second, false);

  receive_update_priorities();
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

  // Clear the resume data since if the syncing fails we propably don't
  // want the old resume data.
  Bencode& resume = m_bencode.insert_key("libtorrent resume", Bencode(Bencode::TYPE_MAP));

  // We're guaranteed that file modification time is correctly updated
  // after this. Though won't help if the files have been delete while
  // we had them open.
  //m_main.content()->entry_list()->sync_all();

  // We sync all chunks in DownloadMain::stop(), so we are guaranteed
  // that it has been called when we arrive here.

  resume.insert_key("bitfield", std::string((char*)m_main.content()->bitfield().begin(), m_main.content()->bitfield().size_bytes()));

  Bencode::List& l = resume.insert_key("files", Bencode(Bencode::TYPE_LIST)).as_list();

  EntryList::iterator sItr = m_main.content()->entry_list()->begin();
  
  // Check the validity of each file, add to the m_hash's ranges if invalid.
  while (sItr != m_main.content()->entry_list()->end()) {
    Bencode& b = *l.insert(l.end(), Bencode(Bencode::TYPE_MAP));

    FileStat fs;

    if (fs.update(sItr->file_meta()->get_path())) {
      l.clear();
      break;
    }

    b.insert_key("mtime", fs.get_mtime());
    b.insert_key("priority", (int)sItr->priority());

    ++sItr;
  }

  // Save the available peer list. Since this function is called when
  // the download is stopped, we know that all the previously
  // connected peers have been copied to the available list.
  std::string peers;
  peers.reserve(m_main.available_list()->size() * sizeof(SocketAddressCompact));
  
  for (AvailableList::const_iterator
	 itr = m_main.available_list()->begin(),
	 last = m_main.available_list()->end();
       itr != last; ++itr)
    peers.append(itr->get_address_compact().c_str(), sizeof(SocketAddressCompact));

  resume.insert_key("peers", peers);
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
  m_hash->get_queue()->remove(get_hash());

  m_main.close();
}

void
DownloadWrapper::start() {
  if (!m_hash->is_checked())
    throw client_error("Tried to start an unchecked download");

  if (!m_main.is_open())
    throw client_error("Tried to start a closed download");

  if (m_main.is_active())
    return;

  m_connectionChunkPassed = signal_chunk_passed().connect(sigc::mem_fun(*m_main.delegator(), &Delegator::done));
  m_connectionChunkFailed = signal_chunk_failed().connect(sigc::mem_fun(m_main.delegator(), &Delegator::redo));

  m_main.start();
}

void
DownloadWrapper::stop() {
  if (!m_main.is_active())
    return;

  m_main.stop();

  m_connectionChunkPassed.disconnect();
  m_connectionChunkFailed.disconnect();
}

bool
DownloadWrapper::is_stopped() const {
  return !m_main.tracker_manager()->is_active();
}

const std::string&
DownloadWrapper::get_hash() const {
  return m_main.tracker_manager()->tracker_info()->get_hash();
}

const std::string&
DownloadWrapper::get_local_id() const {
  return m_main.tracker_manager()->tracker_info()->get_local_id();
}

SocketAddress&
DownloadWrapper::local_address() {
  return m_main.tracker_manager()->tracker_info()->get_local_address();
}

void
DownloadWrapper::set_file_manager(FileManager* f) {
  m_main.content()->entry_list()->slot_insert_filemeta(rak::make_mem_fn(f, &FileManager::insert));
  m_main.content()->entry_list()->slot_erase_filemeta(rak::make_mem_fn(f, &FileManager::erase));
}

void
DownloadWrapper::set_handshake_manager(HandshakeManager* h) {
  m_main.slot_count_handshakes(rak::make_mem_fn(h, &HandshakeManager::size_hash));
  m_main.slot_start_handshake(rak::make_mem_fn(h, &HandshakeManager::add_outgoing));
}

void
DownloadWrapper::set_hash_queue(HashQueue* h) {
  m_hash->set_queue(h);
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
    m_hash->get_queue()->remove(get_hash());
    m_main.content()->clear();

  } else if (!m_main.content()->entry_list()->resize_all()) {
    // We couldn't resize the files, tell the client.
    receive_storage_error("Could not resize files in the torrent.");

    // Do we clear the hash?
  }

  m_signalInitialHash.emit();
}    

void
DownloadWrapper::check_chunk_hash(ChunkHandle handle) {
  // Using HashTorrent's queue temporarily.
  m_hash->get_queue()->push_back(handle, rak::make_mem_fn(this, &DownloadWrapper::receive_hash_done), get_hash());
}

void
DownloadWrapper::receive_hash_done(ChunkHandle handle, std::string h) {
  if (!handle.is_valid())
    throw internal_error("DownloadMain::receive_hash_done(...) called on an invalid chunk.");

  if (m_hash->is_checking()) {
    m_main.content()->receive_chunk_hash(handle->index(), h);
    m_hash->receive_chunkdone();

  } else if (is_open()) {

    if (m_main.content()->receive_chunk_hash(handle->index(), h)) {
      signal_chunk_passed().emit(handle->index());
      m_main.update_endgame();

      if (m_main.content()->is_done())
	m_main.connection_list()->erase_seeders();
    
      m_main.connection_list()->send_finished_chunk(handle->index());

    } else {
      signal_chunk_failed().emit(handle->index());
    }

  } else {
    throw internal_error("DownloadMain::receive_hash_done(...) called but we're not in the right state.");
  }

  m_main.chunk_list()->release(handle);
}  

void
DownloadWrapper::receive_storage_error(const std::string& str) {
  m_main.signal_storage_error().emit(str);
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
DownloadWrapper::receive_update_priorities() {
  Priority* p = &m_main.delegator()->get_select().priority();

  p->clear();

  for (EntryList::iterator itr = m_main.content()->entry_list()->begin(); itr != m_main.content()->entry_list()->end(); ++itr)
    if (itr->range().first != itr->range().second)
      p->add((Priority::Type)itr->priority(), itr->range().first, itr->range().second);

  std::for_each(m_main.connection_list()->begin(), m_main.connection_list()->end(),
		std::mem_fun(&PeerConnectionBase::update_interested));

  Priority::iterator itr = p->begin(Priority::NORMAL);

  while (itr != p->end(Priority::NORMAL) && (itr + 1) != p->end(Priority::NORMAL)) {
    if (itr->second >= (itr + 1)->first)
      throw internal_error("DownloadWrapper::receive_update_priorities() internal range error (normal).");

    ++itr;
  }

  itr = p->begin(Priority::HIGH);

  while (itr != p->end(Priority::HIGH) && (itr + 1) != p->end(Priority::HIGH)) {
    if (itr->second >= (itr + 1)->first)
      throw internal_error("DownloadWrapper::receive_update_priorities() internal range error (high).");

    ++itr;
  }

}

}
