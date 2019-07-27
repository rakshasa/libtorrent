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

#include <iterator>
#include <stdlib.h>
#include <rak/file_stat.h>

#include "data/chunk_list.h"
#include "data/hash_queue.h"
#include "data/hash_torrent.h"
#include "protocol/handshake_manager.h"
#include "protocol/peer_connection_base.h"
#include "torrent/exceptions.h"
#include "torrent/download.h"
#include "torrent/object.h"
#include "torrent/tracker_list.h"
#include "torrent/data/file.h"
#include "torrent/data/file_list.h"
#include "torrent/data/file_manager.h"
#include "torrent/peer/peer.h"
#include "torrent/peer/connection_list.h"
#include "torrent/tracker_controller.h"
#include "torrent/tracker_list.h"
#include "torrent/utils/log.h"

#include "available_list.h"
#include "chunk_selector.h"

#include "download_wrapper.h"

#define LT_LOG_STORAGE_ERRORS(log_fmt, ...)                             \
  lt_log_print_info(LOG_PROTOCOL_STORAGE_ERRORS, this->info(), "storage_errors", log_fmt, __VA_ARGS__);

namespace torrent {

DownloadWrapper::DownloadWrapper() :
  m_main(new DownloadMain),

  m_bencode(NULL),
  m_hashChecker(NULL),
  m_connectionType(0) {

  m_main->delay_download_done().slot()       = std::bind(&download_data::call_download_done, data());
  m_main->delay_partially_done().slot()      = std::bind(&download_data::call_partially_done, data());
  m_main->delay_partially_restarted().slot() = std::bind(&download_data::call_partially_restarted, data());

  m_main->peer_list()->set_info(info());
  m_main->tracker_list()->set_info(info());
  m_main->tracker_controller()->slot_success() = std::bind(&DownloadWrapper::receive_tracker_success, this, std::placeholders::_1);
  m_main->tracker_controller()->slot_failure() = std::bind(&DownloadWrapper::receive_tracker_failed, this, std::placeholders::_1);

  m_main->chunk_list()->slot_storage_error() = std::bind(&DownloadWrapper::receive_storage_error, this, std::placeholders::_1);
}

DownloadWrapper::~DownloadWrapper() {
  if (info()->is_active())
    m_main->stop();

  if (info()->is_open())
    close();

  // If the client wants to do a quick cleanup after calling close, it
  // will need to manually cancel the tracker requests.
  m_main->tracker_controller()->close();

  delete m_hashChecker;
  delete m_bencode;
  delete m_main;
}

void
DownloadWrapper::initialize(const std::string& hash, const std::string& id, int flags) {
  char hashObfuscated[20];
  sha1_salt("req2", 4, hash.c_str(), hash.length(), hashObfuscated);

  info()->mutable_hash().assign(hash.c_str());
  info()->mutable_hash_obfuscated().assign(hashObfuscated);

  info()->mutable_local_id().assign(id.c_str());

  info()->slot_left()      = std::bind(&FileList::left_bytes, m_main->file_list());
  info()->slot_completed() = std::bind(&FileList::completed_bytes, m_main->file_list());

  file_list()->mutable_data()->mutable_hash().assign(hash.c_str());

  m_main->slot_hash_check_add(rak::make_mem_fun(this, &DownloadWrapper::check_chunk_hash));

  // Info hash must be calculate from here on.
  m_hashChecker = new HashTorrent(m_main->chunk_list());

  // Connect various signals and slots.
  m_hashChecker->slot_check_chunk() = std::bind(&DownloadWrapper::check_chunk_hash, this, std::placeholders::_1);
  m_hashChecker->delay_checked().slot() = std::bind(&DownloadWrapper::receive_initial_hash, this, flags);
}

void
DownloadWrapper::close() {
  // Stop the hashing first as we need to make sure all chunks are
  // released when DownloadMain::close() is called.
  m_hashChecker->clear();

  // Clear after m_hashChecker to ensure that the empty hash done signal does
  // not get passed to HashTorrent.
  hash_queue()->remove(data());

  // This could/should be async as we do not care that much if it
  // succeeds or not, any chunks not included in that last
  // hash_resume_save get ignored anyway.
  m_main->chunk_list()->sync_chunks(ChunkList::sync_all | ChunkList::sync_force | ChunkList::sync_sloppy | ChunkList::sync_ignore_error);

  m_main->close();

  // Should this perhaps be in stop?
  priority_queue_erase(&taskScheduler, &m_main->delay_download_done());
  priority_queue_erase(&taskScheduler, &m_main->delay_partially_done());
  priority_queue_erase(&taskScheduler, &m_main->delay_partially_restarted());
}

bool
DownloadWrapper::is_stopped() const {
  return !m_main->tracker_controller()->is_active() && !m_main->tracker_list()->has_active();
}

void
DownloadWrapper::receive_initial_hash(int flags) {
  if (info()->is_active())
    throw internal_error("DownloadWrapper::receive_initial_hash() but we're in a bad state.");

  if (!m_hashChecker->is_checking()) {
    receive_storage_error("Hash checker was unable to map chunk: " + std::string(rak::error_number(m_hashChecker->error_number()).c_str()));

  } else {
    m_hashChecker->confirm_checked();

    if (hash_queue()->has(data()))
      throw internal_error("DownloadWrapper::receive_initial_hash() found a chunk in the HashQueue.");

    // Initialize the ChunkSelector here so that no chunks will be
    // marked by HashTorrent that are not accounted for.
    m_main->chunk_selector()->initialize(m_main->chunk_statistics());
    receive_update_priorities(flags);
  }

  if (data()->slot_initial_hash())
    data()->slot_initial_hash()();
}    

void
DownloadWrapper::receive_hash_done(ChunkHandle handle, const char* hash) {
  if (!handle.is_valid())
    throw internal_error("DownloadWrapper::receive_hash_done(...) called on an invalid chunk.");

  if (!info()->is_open())
    throw internal_error("DownloadWrapper::receive_hash_done(...) called but the download is not open.");

  if (m_hashChecker->is_checking()) {
    
    if (hash == NULL) {
      m_hashChecker->receive_chunk_cleared(handle.index());

    } else {
      if (std::memcmp(hash, chunk_hash(handle.index()), 20) == 0)
        m_main->file_list()->mark_completed(handle.index());

      m_hashChecker->receive_chunkdone(handle.index());
    }

    m_main->chunk_list()->release(&handle, ChunkList::get_dont_log);
    return;
  }

  // If hash == NULL we're clearing the queue, so do nothing.
  if (hash != NULL) {
    if (!m_hashChecker->is_checked())
      throw internal_error("DownloadWrapper::receive_hash_done(...) Was not expecting non-NULL hash.");

    // Receiving chunk hashes after stopping the torrent should be
    // safe.

    if (data()->untouched_bitfield()->get(handle.index()))
      throw internal_error("DownloadWrapper::receive_hash_done(...) received a chunk that isn't set in ChunkSelector.");

    if (std::memcmp(hash, chunk_hash(handle.index()), 20) == 0) {
      bool was_partial = data()->wanted_chunks() != 0;

      m_main->file_list()->mark_completed(handle.index());
      m_main->delegator()->transfer_list()->hash_succeeded(handle.index(), handle.chunk());
      m_main->update_endgame();

      if (m_main->file_list()->is_done()) {
        finished_download();

      } else if (was_partial && data()->wanted_chunks() == 0) {
        priority_queue_erase(&taskScheduler, &m_main->delay_partially_done());
        priority_queue_erase(&taskScheduler, &m_main->delay_partially_restarted());
        priority_queue_insert(&taskScheduler, &m_main->delay_partially_done(), cachedTime);
      }
    
      if (!m_main->have_queue()->empty() && m_main->have_queue()->front().first >= cachedTime)
        m_main->have_queue()->push_front(DownloadMain::have_queue_type::value_type(m_main->have_queue()->front().first + 1, handle.index()));
      else
        m_main->have_queue()->push_front(DownloadMain::have_queue_type::value_type(cachedTime, handle.index()));

    } else {
      // This needs to ensure the chunk is still valid.
      m_main->delegator()->transfer_list()->hash_failed(handle.index(), handle.chunk());
    }
  }

    data()->call_chunk_done(handle.object());
  m_main->chunk_list()->release(&handle);
}  

void
DownloadWrapper::check_chunk_hash(ChunkHandle handle) {
  // TODO: Hack...
  ChunkHandle new_handle = m_main->chunk_list()->get(handle.index(), ChunkList::get_blocking);
  m_main->chunk_list()->release(&handle);

  hash_queue()->push_back(new_handle, data(), std::bind(&DownloadWrapper::receive_hash_done, this, std::placeholders::_1, std::placeholders::_2));
}

void
DownloadWrapper::receive_storage_error(const std::string& str) {
  m_main->stop();
  close();

  m_main->tracker_controller()->disable();
  m_main->tracker_controller()->close();

  LT_LOG_STORAGE_ERRORS("%s", str.c_str());
}

uint32_t
DownloadWrapper::receive_tracker_success(AddressList* l) {
  uint32_t inserted = m_main->peer_list()->insert_available(l);
  m_main->receive_connect_peers();
  m_main->receive_tracker_success();

  rak::slot_list_call(info()->signal_tracker_success());
  return inserted;
}

void
DownloadWrapper::receive_tracker_failed(const std::string& msg) {
  rak::slot_list_call(info()->signal_tracker_failed(), msg);
}

void
DownloadWrapper::receive_tick(uint32_t ticks) {
  // Trigger culling of PeerInfo's every hour. This should be called
  // before the is_open check to ensure that stopped torrents reduce
  // their memory usage.
  if (ticks % 120 == 0)
//   if (ticks % 1 == 0)
    m_main->peer_list()->cull_peers(PeerList::cull_old | PeerList::cull_keep_interesting);

  if (!info()->is_open())
    return;

  // Every 2 minutes.
  if (ticks % 4 == 0) {
    if (info()->is_active()) {
      if (info()->is_pex_enabled()) {
        m_main->do_peer_exchange();

      // If PEX was disabled since the last peer exchange, deactivate it now.
      } else if (info()->is_pex_active()) {
        info()->unset_flags(DownloadInfo::flag_pex_active);

        for (ConnectionList::iterator itr = m_main->connection_list()->begin(); itr != m_main->connection_list()->end(); ++itr)
          (*itr)->m_ptr()->set_peer_exchange(false);
      }
    }

    for (ConnectionList::iterator itr = m_main->connection_list()->begin(); itr != m_main->connection_list()->end(); )
      if (!(*itr)->m_ptr()->receive_keepalive())
        itr = m_main->connection_list()->erase(itr, ConnectionList::disconnect_available);
      else
        itr++;
  }

  DownloadMain::have_queue_type* haveQueue = m_main->have_queue();
  haveQueue->erase(std::find_if(haveQueue->rbegin(), haveQueue->rend(),
                                rak::less(cachedTime - rak::timer::from_seconds(600),
                                             rak::mem_ref(&DownloadMain::have_queue_type::value_type::first))).base(),
                   haveQueue->end());

  m_main->receive_connect_peers();
}

void
DownloadWrapper::receive_update_priorities(int flags) {
  if (m_main->chunk_selector()->empty())
    return;

  data()->mutable_high_priority()->clear();
  data()->mutable_normal_priority()->clear();

  for (FileList::iterator itr = m_main->file_list()->begin(); itr != m_main->file_list()->end(); ++itr) {
      // Unset fallocate flag by default.
      (*itr)->unset_flags(File::flag_fallocate);

    switch ((*itr)->priority()) {
    case PRIORITY_NORMAL:
    {
      if (flags & torrent::Download::open_enable_fallocate)
        (*itr)->set_flags(File::flag_fallocate);

      File::range_type range = (*itr)->range();

      if ((*itr)->has_flags(File::flag_prioritize_first) && range.first != range.second) {
        data()->mutable_high_priority()->insert(range.first, range.first + 1);
        range.first++;
      }

      if ((*itr)->has_flags(File::flag_prioritize_last) && range.first != range.second) {
        data()->mutable_high_priority()->insert(range.second - 1, range.second);
        range.second--;
      }

      data()->mutable_normal_priority()->insert(range);
      break;
    }
    case PRIORITY_HIGH:
      if (flags & torrent::Download::open_enable_fallocate)
        (*itr)->set_flags(File::flag_fallocate);

      data()->mutable_high_priority()->insert((*itr)->range().first, (*itr)->range().second);
      break;
    default:
      break;
    }
  }

  bool was_partial = data()->wanted_chunks() != 0;

  data()->update_wanted_chunks();

  m_main->chunk_selector()->update_priorities();

  std::for_each(m_main->connection_list()->begin(), m_main->connection_list()->end(),
                rak::on(std::mem_fun(&Peer::m_ptr), std::mem_fun(&PeerConnectionBase::update_interested)));

  // The 'partially_done/restarted' signal only gets triggered when a
  // download is active and not completed.
  if (info()->is_active() && !file_list()->is_done() && was_partial != (data()->wanted_chunks() != 0)) {
    priority_queue_erase(&taskScheduler, &m_main->delay_partially_done());
    priority_queue_erase(&taskScheduler, &m_main->delay_partially_restarted());
    
    if (was_partial)
      priority_queue_insert(&taskScheduler, &m_main->delay_partially_done(), cachedTime);
    else
      priority_queue_insert(&taskScheduler, &m_main->delay_partially_restarted(), cachedTime);
  }
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
  if (!m_main->delay_download_done().is_queued())
    priority_queue_insert(&taskScheduler, &m_main->delay_download_done(), cachedTime);

  m_main->connection_list()->erase_seeders();
  info()->mutable_down_rate()->reset_rate();
}

}
