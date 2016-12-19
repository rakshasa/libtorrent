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

#include <cinttypes>

#include "data/block.h"
#include "data/block_list.h"
#include "data/chunk_list.h"
#include "data/hash_queue.h"
#include "data/hash_torrent.h"
#include "download/available_list.h"
#include "download/chunk_selector.h"
#include "download/chunk_statistics.h"
#include "download/download_wrapper.h"
#include "protocol/peer_connection_base.h"
#include "protocol/peer_factory.h"
#include "peer/peer_info.h"
#include "torrent/download/choke_group.h"
#include "torrent/download/choke_queue.h"
#include "torrent/download_info.h"
#include "torrent/data/file.h"
#include "torrent/peer/connection_list.h"
#include "torrent/tracker_controller.h"
#include "torrent/tracker_list.h"
#include "torrent/utils/log.h"

#include "exceptions.h"
#include "download.h"
#include "object.h"
#include "throttle.h"
#include "tracker_list.h"

#define LT_LOG_THIS(log_level, log_fmt, ...)                         \
  lt_log_print_info(LOG_TORRENT_##log_level, m_ptr->info(), "download", log_fmt, __VA_ARGS__);

namespace torrent {

const int DownloadInfo::flag_open;
const int DownloadInfo::flag_active;
const int DownloadInfo::flag_compact;
const int DownloadInfo::flag_accepting_new_peers;
const int DownloadInfo::flag_accepting_seeders;
const int DownloadInfo::flag_private;
const int DownloadInfo::flag_meta_download;
const int DownloadInfo::flag_pex_enabled;
const int DownloadInfo::flag_pex_active;

const int DownloadInfo::public_flags;

const uint32_t DownloadInfo::unlimited;

const DownloadInfo* Download::info() const { return m_ptr->info(); }
const download_data* Download::data() const { return m_ptr->data(); }

void
Download::open(int flags) {
  if (m_ptr->info()->is_open())
    return;

  LT_LOG_THIS(INFO, "Opening torrent: flags:%0x.", flags);

  // Currently always open with no_create, as start will make sure
  // they are created. Need to fix this.
  m_ptr->main()->open(FileList::open_no_create);
  m_ptr->hash_checker()->hashing_ranges().insert(0, m_ptr->main()->file_list()->size_chunks());

  // Mark the files by default to be created and resized. The client
  // should be allowed to pass a flag that will keep the old settings,
  // although loading resume data should really handle everything
  // properly.
  int fileFlags = File::flag_create_queued | File::flag_resize_queued;

  if (flags & open_enable_fallocate)
    fileFlags |= File::flag_fallocate;

  for (FileList::iterator itr = m_ptr->main()->file_list()->begin(), last = m_ptr->main()->file_list()->end(); itr != last; itr++)
    (*itr)->set_flags(fileFlags);
}

void
Download::close(int flags) {
  if (m_ptr->info()->is_active())
    stop(0);

  LT_LOG_THIS(INFO, "Closing torrent: flags:%0x.", flags);
  m_ptr->close();
}

void
Download::start(int flags) {
  DownloadInfo* info = m_ptr->info();

  if (!m_ptr->hash_checker()->is_checked())
    throw internal_error("Tried to start an unchecked download.");

  if (!info->is_open())
    throw internal_error("Tried to start a closed download.");

  if (m_ptr->data()->mutable_completed_bitfield()->empty())
    throw internal_error("Tried to start a download with empty bitfield.");

  if (info->is_active())
    return;

  LT_LOG_THIS(INFO, "Starting torrent: flags:%0x.", flags);

  m_ptr->data()->verify_wanted_chunks("Download::start(...)");

//   file_list()->open(flags);

  // If the FileList::open_no_create flag was not set, our new
  // behavior is to create all zero-length files with
  // flag_queued_create set.
  file_list()->open(flags & ~FileList::open_no_create);

  if (m_ptr->connection_type() == CONNECTION_INITIAL_SEED) {
    if (!m_ptr->main()->start_initial_seeding()) 
      set_connection_type(CONNECTION_SEED);
  }

  m_ptr->main()->start();
  m_ptr->main()->tracker_controller()->enable((flags & start_skip_tracker) ? TrackerController::enable_dont_reset_stats : 0);

  // Reset the uploaded/download baseline when we restart the download
  // so that broken trackers get the right uploaded ratio.
  if (!(flags & start_keep_baseline)) {
    info->set_uploaded_baseline(info->up_rate()->total());
    info->set_completed_baseline(m_ptr->main()->file_list()->completed_bytes());

    lt_log_print_info(LOG_TRACKER_INFO, info,
                      "download", "Setting new baseline on start: uploaded:%" PRIu64 " completed:%" PRIu64 ".",
                      info->uploaded_baseline(), info->completed_baseline());
  }

  if (!(flags & start_skip_tracker))
    m_ptr->main()->tracker_controller()->send_start_event();
}

void
Download::stop(int flags) {
  if (!m_ptr->info()->is_active())
    return;

  LT_LOG_THIS(INFO, "Stopping torrent: flags:%0x.", flags);

  m_ptr->main()->stop();

  if (!(flags & stop_skip_tracker))
    m_ptr->main()->tracker_controller()->send_stop_event();

  m_ptr->main()->tracker_controller()->disable();
}

bool
Download::hash_check(bool tryQuick) {
  if (m_ptr->hash_checker()->is_checking())
    throw internal_error("Download::hash_check(...) called but the hash is already being checked.");

  if (!m_ptr->info()->is_open() || m_ptr->info()->is_active())
    throw internal_error("Download::hash_check(...) called on a closed or active download.");

  if (m_ptr->hash_checker()->is_checked())
    throw internal_error("Download::hash_check(...) called but already hash checked.");

  Bitfield* bitfield = m_ptr->data()->mutable_completed_bitfield();

  LT_LOG_THIS(INFO, "Checking hash: allocated:%i try_quick:%i.", !bitfield->empty(), (int)tryQuick);

  if (bitfield->empty()) {
    // The bitfield still hasn't been allocated, so no resume data was
    // given. 
    bitfield->allocate();
    bitfield->unset_all();

    m_ptr->hash_checker()->hashing_ranges().insert(0, m_ptr->main()->file_list()->size_chunks());
  }

  m_ptr->main()->file_list()->update_completed();

  return m_ptr->hash_checker()->start(tryQuick);
}

// Propably not correct, need to clear content, etc.
void
Download::hash_stop() {
  if (!m_ptr->hash_checker()->is_checking())
    return;

  LT_LOG_THIS(INFO, "Hashing stopped.", 0);

  m_ptr->hash_checker()->hashing_ranges().erase(0, m_ptr->hash_checker()->position());
  m_ptr->hash_queue()->remove(m_ptr->data());

  m_ptr->hash_checker()->clear();
}

bool
Download::is_hash_checked() const {
  return m_ptr->hash_checker()->is_checked();
}

bool
Download::is_hash_checking() const {
  return m_ptr->hash_checker()->is_checking();
}

void
Download::set_pex_enabled(bool enabled) {
  if (enabled)
    m_ptr->info()->set_pex_enabled();
  else
    m_ptr->info()->unset_flags(DownloadInfo::flag_pex_enabled);
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

TrackerController*
Download::tracker_controller() const {
  return m_ptr->main()->tracker_controller();
}

TrackerList*
Download::tracker_list() const {
  return m_ptr->main()->tracker_list();
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
Download::set_chunks_done(uint32_t chunks_done, uint32_t chunks_wanted) {
  if (m_ptr->info()->is_open() || !m_ptr->data()->mutable_completed_bitfield()->empty())
    throw input_error("Download::set_chunks_done(...) Invalid state.");

  chunks_done   = std::min<uint32_t>(chunks_done,   m_ptr->file_list()->size_chunks());
  chunks_wanted = std::min<uint32_t>(chunks_wanted, m_ptr->file_list()->size_chunks() - chunks_done);

  m_ptr->data()->mutable_completed_bitfield()->set_size_set(chunks_done);
  m_ptr->data()->set_wanted_chunks(chunks_wanted);
}

void
Download::set_bitfield(bool allSet) {
  if (m_ptr->hash_checker()->is_checked() || m_ptr->hash_checker()->is_checking())
    throw input_error("Download::set_bitfield(...) Download in invalid state.");

  Bitfield* bitfield = m_ptr->data()->mutable_completed_bitfield();

  bitfield->allocate();

  if (allSet)
    bitfield->set_all();
  else
    bitfield->unset_all();
  
  m_ptr->data()->update_wanted_chunks();
  m_ptr->hash_checker()->hashing_ranges().clear();
}

void
Download::set_bitfield(uint8_t* first, uint8_t* last) {
  if (m_ptr->hash_checker()->is_checked() || m_ptr->hash_checker()->is_checking())
    throw input_error("Download::set_bitfield(...) Download in invalid state.");

  if (std::distance(first, last) != (ptrdiff_t)m_ptr->main()->file_list()->bitfield()->size_bytes())
    throw input_error("Download::set_bitfield(...) Invalid length.");

  Bitfield* bitfield = m_ptr->data()->mutable_completed_bitfield();

  bitfield->allocate();
  std::memcpy(bitfield->begin(), first, bitfield->size_bytes());
  bitfield->update();
  
  m_ptr->data()->update_wanted_chunks();
  m_ptr->hash_checker()->hashing_ranges().clear();
}

void
Download::update_range(int flags, uint32_t first, uint32_t last) {
  if (m_ptr->hash_checker()->is_checked() ||
      m_ptr->hash_checker()->is_checking())
    throw input_error("Download::clear_range(...) Download is hash checked/checking.");

  if (m_ptr->main()->file_list()->bitfield()->empty())
    throw input_error("Download::clear_range(...) Bitfield is empty.");

  if (flags & update_range_recheck)
    m_ptr->hash_checker()->hashing_ranges().insert(first, last);
  
  if (flags & (update_range_clear | update_range_recheck)) {
    m_ptr->data()->mutable_completed_bitfield()->unset_range(first, last);
    m_ptr->data()->update_wanted_chunks();
  }
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
  return m_ptr->main()->choke_group()->up_queue()->size_unchoked();
}

uint32_t
Download::peers_currently_interested() const {
  return m_ptr->main()->choke_group()->up_queue()->size_total();
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

// DEPRECATE
uint32_t
Download::uploads_max() const {
  if (m_ptr->main()->up_group_entry()->max_slots() == DownloadInfo::unlimited)
    return 0;

  return m_ptr->main()->up_group_entry()->max_slots();
}

uint32_t
Download::uploads_min() const {
  // if (m_ptr->main()->up_group_entry()->min_slots() == DownloadInfo::unlimited)
  //   return 0;

  return m_ptr->main()->up_group_entry()->min_slots();
}

uint32_t
Download::downloads_max() const {
  if (m_ptr->main()->down_group_entry()->max_slots() == DownloadInfo::unlimited)
    return 0;

  return m_ptr->main()->down_group_entry()->max_slots();
}

uint32_t
Download::downloads_min() const {
  // if (m_ptr->main()->down_group_entry()->min_slots() == DownloadInfo::unlimited)
  //   return 0;

  return m_ptr->main()->down_group_entry()->min_slots();
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
Download::send_completed() {
  m_ptr->main()->tracker_controller()->send_completed_event();
}

void
Download::manual_request(bool force) {
  m_ptr->main()->tracker_controller()->manual_request(force);
}

void
Download::manual_cancel() {
  m_ptr->main()->tracker_controller()->close();
}

// DEPRECATE
void
Download::set_uploads_max(uint32_t v) {
  if (v > (1 << 16)) 
    throw input_error("Max uploads must be between 0 and 2^16."); 
	 	 
  // For the moment, treat 0 as unlimited. 
  m_ptr->main()->up_group_entry()->set_max_slots(v == 0 ? DownloadInfo::unlimited : v); 
  m_ptr->main()->choke_group()->up_queue()->balance_entry(m_ptr->main()->up_group_entry());
}

void
Download::set_uploads_min(uint32_t v) {
  if (v > (1 << 16)) 
    throw input_error("Min uploads must be between 0 and 2^16."); 
	 	 
  // For the moment, treat 0 as unlimited. 
  m_ptr->main()->up_group_entry()->set_min_slots(v); 
  m_ptr->main()->choke_group()->up_queue()->balance_entry(m_ptr->main()->up_group_entry());
}

void
Download::set_downloads_max(uint32_t v) {
  if (v > (1 << 16)) 
    throw input_error("Max downloads must be between 0 and 2^16."); 
	 	 
  // For the moment, treat 0 as unlimited. 
  m_ptr->main()->down_group_entry()->set_max_slots(v == 0 ? DownloadInfo::unlimited : v); 
  m_ptr->main()->choke_group()->down_queue()->balance_entry(m_ptr->main()->down_group_entry());
}

void
Download::set_downloads_min(uint32_t v) {
  if (v > (1 << 16)) 
    throw input_error("Min downloads must be between 0 and 2^16."); 
	 	 
  // For the moment, treat 0 as unlimited. 
  m_ptr->main()->down_group_entry()->set_min_slots(v); 
  m_ptr->main()->choke_group()->down_queue()->balance_entry(m_ptr->main()->down_group_entry());
}

Download::ConnectionType
Download::connection_type() const {
  return (ConnectionType)m_ptr->connection_type();
}

void
Download::set_connection_type(ConnectionType t) {
  if (m_ptr->info()->is_meta_download()) {
    m_ptr->main()->connection_list()->slot_new_connection(&createPeerConnectionMetadata);
    return;
  }

  switch (t) {
  case CONNECTION_LEECH:
    m_ptr->main()->connection_list()->slot_new_connection(&createPeerConnectionDefault);
    break;
  case CONNECTION_SEED:
    m_ptr->main()->connection_list()->slot_new_connection(&createPeerConnectionSeed);
    break;
  case CONNECTION_INITIAL_SEED:
    if (info()->is_active() && m_ptr->main()->initial_seeding() == NULL)
      throw input_error("Can't switch to initial seeding: download is active.");
    m_ptr->main()->connection_list()->slot_new_connection(&createPeerConnectionInitialSeed);
    break;
  default:
    throw input_error("torrent::Download::set_connection_type(...) received an unknown type.");
  };

  m_ptr->set_connection_type(t);
}

Download::HeuristicType
Download::upload_choke_heuristic() const {
  return (Download::HeuristicType)m_ptr->main()->choke_group()->up_queue()->heuristics();
}

void
Download::set_upload_choke_heuristic(HeuristicType t) {
  if ((choke_queue::heuristics_enum)t >= choke_queue::HEURISTICS_MAX_SIZE)
    throw input_error("Invalid heuristics value.");
  
  m_ptr->main()->choke_group()->up_queue()->set_heuristics((choke_queue::heuristics_enum)t);
}

Download::HeuristicType
Download::download_choke_heuristic() const {
  return (Download::HeuristicType)m_ptr->main()->choke_group()->down_queue()->heuristics();
}

void
Download::set_download_choke_heuristic(HeuristicType t) {
  if ((choke_queue::heuristics_enum)t >= choke_queue::HEURISTICS_MAX_SIZE)
    throw input_error("Invalid heuristics value.");
  
  m_ptr->main()->choke_group()->down_queue()->set_heuristics((choke_queue::heuristics_enum)t);
}

void
Download::update_priorities() {
  m_ptr->receive_update_priorities();
}

void
Download::add_peer(const sockaddr* sa, int port) {
  if (m_ptr->info()->is_private())
    return;

  rak::socket_address sa_port = *rak::socket_address::cast_from(sa);
  sa_port.set_port(port);
  m_ptr->main()->add_peer(sa_port);
}

DownloadMain* Download::main() { return m_ptr->main(); }

}
