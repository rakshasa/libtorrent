#include "config.h"

#include "download/download_wrapper.h"

#include "data/chunk_list.h"
#include "data/hash_queue.h"
#include "data/hash_torrent.h"
#include "download/chunk_selector.h"
#include "protocol/handshake_manager.h"
#include "protocol/peer_connection_base.h"
#include "torrent/data/file.h"
#include "torrent/data/file_list.h"
#include "torrent/data/file_manager.h"
#include "torrent/download_info.h"
#include "torrent/peer/connection_list.h"
#include "torrent/peer/peer.h"
#include "torrent/tracker/manager.h"
#include "torrent/utils/log.h"
#include "tracker/thread_tracker.h"
#include "tracker/tracker_list.h"
#include "utils/functional.h"
#include "utils/sha1.h"

#define LT_LOG_THIS(log_fmt, ...)                                       \
  lt_log_print_info(LOG_TORRENT_INFO, this->info(), "download", log_fmt, __VA_ARGS__);
#define LT_LOG_STORAGE_ERRORS(log_fmt, ...)                             \
  lt_log_print_info(LOG_PROTOCOL_STORAGE_ERRORS, this->info(), "storage_errors", log_fmt, __VA_ARGS__);

namespace torrent {

DownloadWrapper::DownloadWrapper()
  : m_main(new DownloadMain) {

  m_main->delay_download_done().slot()       = [this] { data()->call_download_done(); };
  m_main->delay_partially_done().slot()      = [this] { data()->call_partially_done(); };
  m_main->delay_partially_restarted().slot() = [this] { data()->call_partially_restarted(); };

  m_main->peer_list()->set_info(info());
  m_main->tracker_list()->set_info(info());

  m_main->chunk_list()->slot_storage_error() = [this](const auto& str) { receive_storage_error(str); };
}

DownloadWrapper::~DownloadWrapper() {
  if (info()->is_active())
    m_main->stop();

  if (info()->is_open())
    close();

  // Check if needed.
  m_main->connection_list()->clear();
  m_main->tracker_list()->clear();

  if (m_main->tracker_controller().is_valid()) {
    // If the client wants to do a quick cleanup after calling close, it
    // will need to manually cancel the tracker requests.
    m_main->tracker_controller().close();

    thread_tracker()->tracker_manager()->remove_controller(m_main->tracker_controller());
    m_main->tracker_controller().get_shared().reset();
  }
}

void
DownloadWrapper::initialize(const std::string& hash, const std::string& id, uint32_t tracker_key) {
  char hashObfuscated[20];
  sha1_salt("req2", 4, hash.c_str(), hash.length(), hashObfuscated);

  info()->mutable_hash().assign(hash.c_str());
  info()->mutable_hash_obfuscated().assign(hashObfuscated);

  info()->mutable_local_id().assign(id.c_str());

  info()->slot_left()      = [this] { return m_main->file_list()->left_bytes(); };
  info()->slot_completed() = [this] { return m_main->file_list()->completed_bytes(); };

  file_list()->mutable_data()->mutable_hash().assign(hash.c_str());

  m_main->tracker_list()->set_key(tracker_key);

  m_main->slot_hash_check_add([this](torrent::ChunkHandle handle) { return check_chunk_hash(handle, false); });

  // Info hash must be calculate from here on.
  m_hash_checker = std::make_unique<HashTorrent>(m_main->chunk_list());

  // Connect various signals and slots.
  m_hash_checker->slot_check_chunk()     = [this](auto h) { check_chunk_hash(h, true); };
  m_hash_checker->delay_checked().slot() = [this] { receive_initial_hash(); };

  m_main->post_initialize();

  m_main->tracker_controller().set_slots([this](auto l) { return receive_tracker_success(l); },
                                         [this](auto& m) { return receive_tracker_failed(m); });
}

void
DownloadWrapper::close() {
  // Stop the hashing first as we need to make sure all chunks are
  // released when DownloadMain::close() is called.
  m_hash_checker->clear();

  // Clear after m_hash_checker to ensure that the empty hash done signal does
  // not get passed to HashTorrent.
  hash_queue()->remove(data());

  // This could/should be async as we do not care that much if it
  // succeeds or not, any chunks not included in that last
  // hash_resume_save get ignored anyway.
  m_main->chunk_list()->sync_chunks(ChunkList::sync_all | ChunkList::sync_force | ChunkList::sync_sloppy | ChunkList::sync_ignore_error);

  m_main->close();

  // Should this perhaps be in stop?
  this_thread::scheduler()->erase(&m_main->delay_download_done());
  this_thread::scheduler()->erase(&m_main->delay_partially_done());
  this_thread::scheduler()->erase(&m_main->delay_partially_restarted());
}

bool
DownloadWrapper::is_stopped() const {
  return !m_main->tracker_controller().is_active() && !m_main->tracker_list()->has_active();
}

void
DownloadWrapper::receive_initial_hash() {
  if (info()->is_active())
    throw internal_error("DownloadWrapper::receive_initial_hash() but we're in a bad state.");

  if (!m_hash_checker->is_checking()) {
    receive_storage_error("Hash checker was unable to map chunk: " + std::string(rak::error_number(m_hash_checker->error_number()).c_str()));

  } else {
    m_hash_checker->confirm_checked();

    if (hash_queue()->has(data()))
      throw internal_error("DownloadWrapper::receive_initial_hash() found a chunk in the HashQueue.");

    // Initialize the ChunkSelector here so that no chunks will be
    // marked by HashTorrent that are not accounted for.
    m_main->chunk_selector()->initialize(m_main->chunk_statistics());

    receive_update_priorities();
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

  if (m_hash_checker->is_checking()) {
    if (hash == NULL) {
      m_hash_checker->receive_chunk_cleared(handle.index());

    } else {
      if (std::memcmp(hash, chunk_hash(handle.index()), 20) == 0)
        m_main->file_list()->mark_completed(handle.index());

      m_hash_checker->receive_chunkdone(handle.index());
    }

    m_main->chunk_list()->release(&handle, ChunkList::release_dont_log);
    return;
  }

  // If hash == NULL we're clearing the queue, so do nothing.
  if (hash != NULL) {
    if (!m_hash_checker->is_checked())
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
        this_thread::scheduler()->erase(&m_main->delay_partially_restarted());
        this_thread::scheduler()->update_wait_for(&m_main->delay_partially_done(), 0us);
      }

      if (!m_main->have_queue()->empty() && m_main->have_queue()->front().first >= this_thread::cached_time())
        m_main->have_queue()->emplace_front(m_main->have_queue()->front().first + 1us, handle.index());
      else
        m_main->have_queue()->emplace_front(this_thread::cached_time(), handle.index());

    } else {
      // This needs to ensure the chunk is still valid.
      m_main->delegator()->transfer_list()->hash_failed(handle.index(), handle.chunk());
    }
  }

  data()->call_chunk_done(handle.object());
  m_main->chunk_list()->release(&handle, ChunkList::release_default);
}

void
DownloadWrapper::check_chunk_hash(ChunkHandle handle, bool hashing) {
  // TODO: Hack...
  auto flags = ChunkList::get_blocking;

  if (hashing)
    flags = flags | ChunkList::get_hashing;
  else
    flags = flags | ChunkList::get_not_hashing;

  ChunkHandle new_handle = m_main->chunk_list()->get(handle.index(), flags);
  m_main->chunk_list()->release(&handle, ChunkList::release_default);

  hash_queue()->push_back(new_handle, data(), [this](auto c, auto h) { receive_hash_done(c, h); });
}

void
DownloadWrapper::receive_storage_error(const std::string& str) {
  m_main->stop();
  close();

  m_main->tracker_controller().disable();
  m_main->tracker_controller().close();

  LT_LOG_STORAGE_ERRORS("%s", str.c_str());
}

uint32_t
DownloadWrapper::receive_tracker_success(AddressList* l) {
  uint32_t inserted = m_main->peer_list()->insert_available(l);
  m_main->receive_connect_peers();
  m_main->receive_tracker_success();

  ::utils::slot_list_call(info()->signal_tracker_success());
  return inserted;
}

void
DownloadWrapper::receive_tracker_failed(const std::string& msg) {
  ::utils::slot_list_call(info()->signal_tracker_failed(), msg);
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

        for (auto& connection : *m_main->connection_list())
          connection->m_ptr()->set_peer_exchange(false);
      }
    }

    for (auto itr = m_main->connection_list()->begin(); itr != m_main->connection_list()->end();)
      if (!(*itr)->m_ptr()->receive_keepalive())
        itr = m_main->connection_list()->erase(itr, ConnectionList::disconnect_available);
      else
        itr++;
  }

  auto have_queue = m_main->have_queue();

  have_queue->erase(std::find_if(have_queue->rbegin(),
                                 have_queue->rend(), [](auto& p) {
                                     return this_thread::cached_time() - 600s < p.first;
                                   }).base(),
                    have_queue->end());

  m_main->receive_connect_peers();
}

void
DownloadWrapper::receive_update_priorities() {
  LT_LOG_THIS("update priorities: chunks_selected:%" PRIu32 " wanted_chunks:%" PRIu32,
              m_main->chunk_selector()->size(), data()->wanted_chunks());

  data()->mutable_high_priority()->clear();
  data()->mutable_normal_priority()->clear();

  for (auto& file : *m_main->file_list()) {
    switch (file->priority()) {
    case PRIORITY_NORMAL:
    {
      File::range_type range = file->range();

      if (file->has_flags(File::flag_prioritize_first) && range.first != range.second) {
        data()->mutable_high_priority()->insert(range.first, range.first + 1);
        range.first++;
      }

      if (file->has_flags(File::flag_prioritize_last) && range.first != range.second) {
        data()->mutable_high_priority()->insert(range.second - 1, range.second);
        range.second--;
      }

      data()->mutable_normal_priority()->insert(range);
      break;
    }
    case PRIORITY_HIGH:
      data()->mutable_high_priority()->insert(file->range().first, file->range().second);
      break;
    default:
      break;
    }
  }

  bool was_partial = data()->wanted_chunks() != 0;
  data()->update_wanted_chunks();

  m_main->chunk_selector()->update_priorities();

  for (const auto& peer : *m_main->connection_list()) {
    peer->m_ptr()->update_interested();
  }

  // The 'partially_done/restarted' signal only gets triggered when a
  // download is active and not completed.
  if (info()->is_active() && !file_list()->is_done() && was_partial != (data()->wanted_chunks() != 0)) {
    this_thread::scheduler()->erase(&m_main->delay_partially_done());
    this_thread::scheduler()->erase(&m_main->delay_partially_restarted());

    if (was_partial)
      this_thread::scheduler()->wait_for(&m_main->delay_partially_done(), 0us);
    else
      this_thread::scheduler()->wait_for(&m_main->delay_partially_restarted(), 0us);
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
  this_thread::scheduler()->update_wait_for(&m_main->delay_download_done(), 0us);

  m_main->connection_list()->erase_seeders();
  info()->mutable_down_rate()->reset_rate();
}

} // namespace torrent
