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

#include <rak/functional.h>
#include <sigc++/adaptors/bind.h>
#include <sigc++/adaptors/hide.h>

#include "data/block.h"
#include "data/block_list.h"
#include "data/chunk_list.h"
#include "data/hash_queue.h"
#include "data/hash_torrent.h"
#include "download/available_list.h"
#include "download/choke_manager.h"
#include "download/chunk_selector.h"
#include "download/chunk_statistics.h"
#include "download/download_wrapper.h"
#include "protocol/peer_connection_base.h"
#include "protocol/peer_factory.h"
#include "download/download_info.h"
#include "peer/peer_info.h"
#include "tracker/tracker_manager.h"
#include "torrent/data/file.h"
#include "torrent/peer/connection_list.h"

#include "exceptions.h"
#include "download.h"
#include "object.h"
#include "throttle.h"
#include "tracker_list.h"

namespace torrent {

void
Download::open(int flags) {
  if (m_ptr->info()->is_open())
    return;

  // Currently always open with no_create, as start will make sure
  // they are created. Need to fix this.
  m_ptr->main()->open(FileList::open_no_create);
  m_ptr->hash_checker()->ranges().insert(0, m_ptr->main()->file_list()->size_chunks());

  // Mark the files by default to be created and resized. The client
  // should be allowed to pass a flag that will keep the old settings,
  // although loading resume data should really handle everything
  // properly.
  for (FileList::iterator itr = m_ptr->main()->file_list()->begin(), last = m_ptr->main()->file_list()->end(); itr != last; itr++)
    (*itr)->set_flags(File::flag_create_queued | File::flag_resize_queued);
}

void
Download::close(int flags) {
  if (m_ptr->info()->is_active())
    stop(0);

  m_ptr->close();
}

void
Download::start(int flags) {
  if (!m_ptr->hash_checker()->is_checked())
    throw internal_error("Tried to start an unchecked download.");

  if (!m_ptr->info()->is_open())
    throw internal_error("Tried to start a closed download.");

  if (m_ptr->info()->is_active())
    return;

//   file_list()->open(flags);
  file_list()->open(flags & ~FileList::open_no_create);

  m_ptr->main()->start();
  m_ptr->main()->tracker_manager()->set_active(true);

  // Reset the uploaded/download baseline when we restart the download
  // so that broken trackers get the right uploaded ratio.
  if (!(flags & start_keep_baseline)) {
    m_ptr->info()->set_uploaded_baseline(m_ptr->info()->up_rate()->total());
    m_ptr->info()->set_completed_baseline(m_ptr->info()->slot_completed()());
  }

  if (flags & start_skip_tracker)
    // If tracker_manager isn't active and nothing is sent, it will
    // stay stuck.
    m_ptr->main()->tracker_manager()->send_later();
  else
    m_ptr->main()->tracker_manager()->send_start();
}

void
Download::stop(int flags) {
  if (!m_ptr->info()->is_active())
    return;

  m_ptr->main()->stop();
  m_ptr->main()->tracker_manager()->set_active(false);

  if (!(flags & stop_skip_tracker))
    m_ptr->main()->tracker_manager()->send_stop();
}

bool
Download::hash_check(bool tryQuick) {
  if (m_ptr->hash_checker()->is_checking())
    throw internal_error("Download::hash_check(...) called but the hash is already being checked.");

  if (!m_ptr->info()->is_open() || m_ptr->info()->is_active())
    throw internal_error("Download::hash_check(...) called on a closed or active download.");

  if (m_ptr->hash_checker()->is_checked())
    throw internal_error("Download::hash_check(...) called but already hash checked.");

  Bitfield* bitfield = m_ptr->main()->file_list()->mutable_bitfield();

  if (bitfield->empty()) {
    // The bitfield still hasn't been allocated, so no resume data was
    // given. 
    bitfield->allocate();
    bitfield->unset_all();

    m_ptr->hash_checker()->ranges().insert(0, m_ptr->main()->file_list()->size_chunks());
  }

  m_ptr->main()->file_list()->update_completed();

  return m_ptr->hash_checker()->start(tryQuick);
}

// Propably not correct, need to clear content, etc.
void
Download::hash_stop() {
  if (!m_ptr->hash_checker()->is_checking())
    return;

  m_ptr->hash_checker()->ranges().erase(0, m_ptr->hash_checker()->position());
  m_ptr->hash_queue()->remove(m_ptr);

  m_ptr->hash_checker()->clear();
}

bool
Download::is_open() const {
  return m_ptr->info()->is_open();
}

bool
Download::is_active() const {
  return m_ptr->info()->is_active();
}

bool
Download::is_hash_checked() const {
  return m_ptr->hash_checker()->is_checked();
}

bool
Download::is_hash_checking() const {
  return m_ptr->hash_checker()->is_checking();
}

bool
Download::is_private() const {
  return m_ptr->info()->is_private();
}

bool
Download::is_pex_active() const {
  return m_ptr->info()->is_pex_active();
}

bool
Download::is_pex_enabled() const {
  return m_ptr->info()->is_pex_enabled();
}

void
Download::set_pex_enabled(bool enabled) {
  m_ptr->info()->set_pex_enabled(enabled);
}

const std::string&
Download::name() const {
  if (m_ptr == NULL)
    throw internal_error("Download::name() m_ptr == NULL.");

  return m_ptr->info()->name();
}

const HashString&
Download::info_hash() const {
  return m_ptr->info()->hash();
}

const HashString&
Download::info_hash_obfuscated() const {
  return m_ptr->info()->hash_obfuscated();
}

const HashString&
Download::local_id() const {
  return m_ptr->info()->local_id();
}

uint32_t
Download::creation_date() const {
  if (m_ptr->bencode()->has_key_value("creation date"))
    return m_ptr->bencode()->get_key_value("creation date");
  else
    return 0;
}

Object*
Download::bencode() {
  return m_ptr->bencode();
}

const Object*
Download::bencode() const {
  return m_ptr->bencode();
}

FileList*
Download::file_list() const {
  return m_ptr->main()->file_list();
}

TrackerList*
Download::tracker_list() const {
  return m_ptr->main()->tracker_manager()->container();
}

PeerList*
Download::peer_list() {
  return m_ptr->main()->peer_list();
}

const PeerList*
Download::peer_list() const {
  return m_ptr->main()->peer_list();
}

const TransferList*
Download::transfer_list() const {
  return m_ptr->main()->delegator()->transfer_list();
}

ConnectionList*
Download::connection_list() {
  return m_ptr->main()->connection_list();
}

const ConnectionList*
Download::connection_list() const {
  return m_ptr->main()->connection_list();
}

Rate*
Download::down_rate() {
  return m_ptr->info()->down_rate();
}

const Rate*
Download::down_rate() const {
  return m_ptr->info()->down_rate();
}

Rate*
Download::mutable_down_rate() {
  return m_ptr->info()->down_rate();
}

Rate*
Download::up_rate() {
  return m_ptr->info()->up_rate();
}

const Rate*
Download::up_rate() const {
  return m_ptr->info()->up_rate();
}

Rate*
Download::mutable_up_rate() {
  return m_ptr->info()->up_rate();
}

Rate*
Download::skip_rate() {
  return m_ptr->info()->skip_rate();
}

const Rate*
Download::skip_rate() const {
  return m_ptr->info()->skip_rate();
}

Rate*
Download::mutable_skip_rate() {
  return m_ptr->info()->skip_rate();
}

uint64_t
Download::bytes_done() const {
  uint64_t a = 0;
 
  Delegator* d = m_ptr->main()->delegator();

  for (TransferList::const_iterator itr1 = d->transfer_list()->begin(), last1 = d->transfer_list()->end(); itr1 != last1; ++itr1)
    for (BlockList::const_iterator itr2 = (*itr1)->begin(), last2 = (*itr1)->end(); itr2 != last2; ++itr2)
      if (itr2->is_finished())
        a += itr2->piece().length();
  
  return a + m_ptr->main()->file_list()->completed_bytes();
}

uint32_t 
Download::chunks_hashed() const {
  return m_ptr->hash_checker()->position();
}

const uint8_t*
Download::chunks_seen() const {
  return !m_ptr->main()->chunk_statistics()->empty() ? &*m_ptr->main()->chunk_statistics()->begin() : NULL;
}

void
Download::set_chunks_done(uint32_t chunks) {
  if (m_ptr->info()->is_open())
    throw input_error("Download::set_chunks_done(...) Download is open.");

  m_ptr->main()->file_list()->mutable_bitfield()->set_size_set(chunks);
}

void
Download::set_bitfield(bool allSet) {
  if (m_ptr->hash_checker()->is_checked() || m_ptr->hash_checker()->is_checking())
    throw input_error("Download::set_bitfield(...) Download in invalid state.");

  Bitfield* bitfield = m_ptr->main()->file_list()->mutable_bitfield();

  bitfield->allocate();

  if (allSet)
    bitfield->set_all();
  else
    bitfield->unset_all();
  
  m_ptr->hash_checker()->ranges().clear();
}

void
Download::set_bitfield(uint8_t* first, uint8_t* last) {
  if (m_ptr->hash_checker()->is_checked() || m_ptr->hash_checker()->is_checking())
    throw input_error("Download::set_bitfield(...) Download in invalid state.");

  if (std::distance(first, last) != (ptrdiff_t)m_ptr->main()->file_list()->bitfield()->size_bytes())
    throw input_error("Download::set_bitfield(...) Invalid length.");

  Bitfield* bitfield = m_ptr->main()->file_list()->mutable_bitfield();

  bitfield->allocate();
  std::memcpy(bitfield->begin(), first, bitfield->size_bytes());
  bitfield->update();
  
  m_ptr->hash_checker()->ranges().clear();
}

void
Download::update_range(int flags, uint32_t first, uint32_t last) {
  if (m_ptr->hash_checker()->is_checked() ||
      m_ptr->hash_checker()->is_checking() ||
      m_ptr->main()->file_list()->bitfield()->empty())
    throw input_error("Download::clear_range(...) Download in invalid state.");

  if (flags & update_range_recheck)
    m_ptr->hash_checker()->ranges().insert(first, last);
  
  if (flags & (update_range_clear | update_range_recheck))
    m_ptr->main()->file_list()->mutable_bitfield()->unset_range(first, last);
}
 
void
Download::sync_chunks() {
  m_ptr->main()->chunk_list()->sync_chunks(ChunkList::sync_all | ChunkList::sync_force);
}

uint32_t
Download::peers_complete() const {
  return m_ptr->main()->chunk_statistics()->complete();
}

uint32_t
Download::peers_accounted() const {
  return m_ptr->main()->chunk_statistics()->accounted();
}

uint32_t
Download::peers_currently_unchoked() const {
  return m_ptr->main()->upload_choke_manager()->size_unchoked();
}

uint32_t
Download::peers_currently_interested() const {
  return m_ptr->main()->upload_choke_manager()->size_total();
}

uint32_t
Download::size_pex() const {
  return m_ptr->main()->info()->size_pex();
}

uint32_t
Download::max_size_pex() const {
  return m_ptr->main()->info()->max_size_pex();
}

bool
Download::accepting_new_peers() const {
  return m_ptr->info()->is_accepting_new_peers();
}

uint32_t
Download::uploads_max() const {
  return m_ptr->main()->upload_choke_manager()->max_unchoked();
}

void
Download::set_upload_throttle(Throttle* t) {
  if (m_ptr->info()->is_active())
    throw internal_error("Download::set_upload_throttle() called on active download.");

  m_ptr->main()->set_upload_throttle(t->throttle_list());
}

void
Download::set_download_throttle(Throttle* t) {
  if (m_ptr->info()->is_active())
    throw internal_error("Download::set_download_throttle() called on active download.");

  m_ptr->main()->set_download_throttle(t->throttle_list());
}
  
void
Download::set_uploads_max(uint32_t v) {
  if (v > (1 << 16)) 
    throw input_error("Max uploads must be between 0 and 2^16."); 
	 	 
  // For the moment, treat 0 as unlimited. 
  m_ptr->main()->upload_choke_manager()->set_max_unchoked(v == 0 ? ChokeManager::unlimited : v); 
  m_ptr->main()->upload_choke_manager()->balance();
}

Download::ConnectionType
Download::connection_type() const {
  return (ConnectionType)m_ptr->connection_type();
}

void
Download::set_connection_type(ConnectionType t) {
  switch (t) {
  case CONNECTION_LEECH:
    m_ptr->main()->connection_list()->slot_new_connection(&createPeerConnectionDefault);
    break;
  case CONNECTION_SEED:
    m_ptr->main()->connection_list()->slot_new_connection(&createPeerConnectionSeed);
    break;
  default:
    throw input_error("torrent::Download::set_connection_type(...) received an unknown type.");
  };

  m_ptr->set_connection_type(t);
}

void
Download::update_priorities() {
  m_ptr->receive_update_priorities();
}

void
Download::disconnect_peer(Peer* p) {
  m_ptr->main()->connection_list()->erase(p->m_ptr(), 0);
}

sigc::connection
Download::signal_download_done(Download::slot_void_type s) {
  return m_ptr->signal_download_done().connect(s);
}

sigc::connection
Download::signal_hash_done(Download::slot_void_type s) {
  return m_ptr->signal_initial_hash().connect(s);
}

sigc::connection
Download::signal_tracker_succeded(Download::slot_void_type s) {
  return m_ptr->signal_tracker_success().connect(s);
}

sigc::connection
Download::signal_tracker_failed(Download::slot_string_type s) {
  return m_ptr->signal_tracker_failed().connect(s);
}

sigc::connection
Download::signal_tracker_dump(Download::slot_dump_type s) {
  return m_ptr->info()->signal_tracker_dump().connect(s);
}

sigc::connection
Download::signal_chunk_passed(Download::slot_chunk_type s) {
  return m_ptr->signal_chunk_passed().connect(s);
}

sigc::connection
Download::signal_chunk_failed(Download::slot_chunk_type s) {
  return m_ptr->signal_chunk_failed().connect(s);
}

sigc::connection
Download::signal_network_log(slot_string_type s) {
  return m_ptr->info()->signal_network_log().connect(s);
}

sigc::connection
Download::signal_storage_error(slot_string_type s) {
  return m_ptr->info()->signal_storage_error().connect(s);
}

}
