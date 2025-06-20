#include "config.h"

#include "download/download_main.h"

#include <cassert>
#include <cstring>

#include "data/chunk_list.h"
#include "download/available_list.h"
#include "download/chunk_selector.h"
#include "download/chunk_statistics.h"
#include "download/download_wrapper.h"
#include "manager.h"
#include "protocol/extensions.h"
#include "protocol/handshake_manager.h"
#include "protocol/initial_seed.h"
#include "protocol/peer_connection_base.h"
#include "protocol/peer_factory.h"
#include "torrent/data/file_list.h"
#include "torrent/download.h"
#include "torrent/download/choke_queue.h"
#include "torrent/download/download_manager.h"
#include "torrent/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/peer/connection_list.h"
#include "torrent/peer/peer.h"
#include "torrent/peer/peer_info.h"
#include "torrent/throttle.h"
#include "torrent/tracker/manager.h"
#include "torrent/utils/log.h"
#include "tracker/thread_tracker.h"
#include "tracker/tracker_controller.h"
#include "tracker/tracker_list.h"

#define LT_LOG_THIS(log_level, log_fmt, ...)                         \
  lt_log_print_info(LOG_TORRENT_##log_level, m_ptr->info(), "download", log_fmt, __VA_ARGS__);

namespace torrent {

DownloadMain::DownloadMain()
  : m_info(new DownloadInfo),
    m_tracker_list(new TrackerList),

    m_chunkList(new ChunkList),
    m_chunkSelector(new ChunkSelector(file_list()->mutable_data())),
    m_chunkStatistics(new ChunkStatistics),
    m_connectionList(new ConnectionList(this)) {

  m_info->set_load_date(utils::cast_seconds(utils::time_since_epoch()).count());

  // Only set trivial values here, the rest is done in DownloadWrapper.

  m_delegator.slot_chunk_find() = [this](auto pc, auto prio) { return m_chunkSelector->find(pc, prio); };
  m_delegator.slot_chunk_size() = [this](auto i) { return file_list()->chunk_index_size(i); };

  m_delegator.transfer_list()->slot_canceled()  = [this](auto i) { m_chunkSelector->not_using_index(i); };
  m_delegator.transfer_list()->slot_queued()    = [this](auto i) { m_chunkSelector->using_index(i); };
  m_delegator.transfer_list()->slot_completed() = [this](auto i) { receive_chunk_done(i); };
  m_delegator.transfer_list()->slot_corrupt()   = [this](auto i) { receive_corrupt_chunk(i); };

  m_delay_disconnect_peers.slot() = [this] { m_connectionList->disconnect_queued(); };
  m_task_tracker_request.slot()     = [this] { receive_tracker_request(); };

  m_chunkList->set_data(file_list()->mutable_data());

  m_chunkList->slot_create_chunk() = [this](uint32_t index, int prot) {
      return file_list()->create_chunk_index(index, prot);
    };
  m_chunkList->slot_create_hashing_chunk() = [this](uint32_t index, int prot) {
      return file_list()->create_hashing_chunk_index(index, prot);
    };
  m_chunkList->slot_free_diskspace() = [this]() {
      return file_list()->free_diskspace();
    };
}

DownloadMain::~DownloadMain() {
  assert(!m_task_tracker_request.is_scheduled() && "DownloadMain::~DownloadMain(): m_task_tracker_request is scheduled.");

  assert(m_info->size_pex() == 0 && "DownloadMain::~DownloadMain(): m_info->size_pex() != 0.");

  delete m_tracker_list;
  delete m_connectionList;

  delete m_chunkStatistics;
  delete m_chunkList;
  delete m_chunkSelector;
  delete m_info;

  m_ut_pex_delta.clear();
  m_ut_pex_initial.clear();
}

void
DownloadMain::post_initialize() {
  auto tracker_controller = std::make_shared<TrackerController>(m_tracker_list);

  m_tracker_list->slot_success()          = [tracker_controller](const auto& t, auto al) { return tracker_controller->receive_success(t, al); };
  m_tracker_list->slot_failure()          = [tracker_controller](const auto& t, const auto& str) { tracker_controller->receive_failure(t, str); };
  m_tracker_list->slot_scrape_success()   = [tracker_controller](const auto& t) { tracker_controller->receive_scrape(t); };
  m_tracker_list->slot_tracker_enabled()  = [tracker_controller](const auto& t) { tracker_controller->receive_tracker_enabled(t); };
  m_tracker_list->slot_tracker_disabled() = [tracker_controller](const auto& t) { tracker_controller->receive_tracker_disabled(t); };

  // TODO: Move tracker list to manager, and add the proper barrier for slots.
  m_tracker_controller = thread_tracker()->tracker_manager()->add_controller(info(), std::move(tracker_controller));
}

std::pair<ThrottleList*, ThrottleList*>
DownloadMain::throttles(const sockaddr* sa) {
  ThrottlePair pair = ThrottlePair(NULL, NULL);

  if (manager->connection_manager()->address_throttle())
    pair = manager->connection_manager()->address_throttle()(sa);

  return std::make_pair(pair.first == NULL ? upload_throttle() : pair.first->throttle_list(),
                        pair.second == NULL ? download_throttle() : pair.second->throttle_list());
}

void
DownloadMain::open(int flags) {
  if (info()->is_open())
    throw internal_error("Tried to open a download that is already open");

  // TODO: Move file_list open calls to DownloadMain.
  file_list()->open(true, (flags & FileList::open_no_create));

  m_chunkList->resize(file_list()->size_chunks());
  m_chunkStatistics->initialize(file_list()->size_chunks());

  info()->set_flags(DownloadInfo::flag_open);
}

void
DownloadMain::close() {
  if (info()->is_active())
    throw internal_error("Tried to close an active download");

  if (!info()->is_open())
    return;

  info()->unset_flags(DownloadInfo::flag_open);

  // Don't close the tracker manager here else it will cause STOPPED
  // requests to be lost. TODO: Check that this is valid.
//   m_trackerManager->close();

  m_delegator.transfer_list()->clear();

  file_list()->close();

  // Clear the chunklist last as it requires all referenced chunks to
  // be released.
  m_chunkStatistics->clear();
  m_chunkList->clear();
  m_chunkSelector->cleanup();
}

void DownloadMain::start(int flags) {
  if (!info()->is_open())
    throw internal_error("Tried to start a closed download");

  if (info()->is_active())
    throw internal_error("Tried to start an active download");

  // Close and clear open files to ensure hashing file/mmap advise is cleared.
  //
  // We should not need to clear chunks after hash checking the whole torrent as those chunk handle
  // references go to zero.
  file_list()->close_all_files();

  // If the FileList::open_no_create flag was not set, our new
  // behavior is to create all zero-length files with
  // flag_queued_create set.
  file_list()->open(false, (flags & ~FileList::open_no_create));

  info()->set_flags(DownloadInfo::flag_active);
  chunk_list()->set_flags(ChunkList::flag_active);

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
  info()->unset_flags(DownloadInfo::flag_active);
  chunk_list()->unset_flags(ChunkList::flag_active);

  m_slot_stop_handshakes(this);
  connection_list()->erase_remaining(connection_list()->begin(), ConnectionList::disconnect_available);

  m_initial_seeding.reset();

  this_thread::scheduler()->erase(&m_delay_disconnect_peers);
  this_thread::scheduler()->erase(&m_task_tracker_request);

  if (info()->upload_unchoked() != 0 || info()->download_unchoked() != 0)
    throw internal_error("DownloadMain::stop(): info()->upload_unchoked() != 0 || info()->download_unchoked() != 0.");
}

bool
DownloadMain::start_initial_seeding() {
  if (!file_list()->is_done())
    return false;

  m_initial_seeding = std::make_unique<InitialSeeding>(this);
  return true;
}

void
DownloadMain::initial_seeding_done(PeerConnectionBase* pcb) {
  if (m_initial_seeding == nullptr)
    throw internal_error("DownloadMain::initial_seeding_done called when not initial seeding.");

  // Close all connections but the currently active one (pcb), that
  // one will be closed by throw close_connection() later.
  //
  // When calling initial_seed()->new_peer(...) the 'pcb' won't be in
  // the connection list, so don't treat it as an error. Make sure to
  // catch close_connection() at the caller of new_peer(...) and just
  // close the filedesc before proceeding as normal.
  auto pcb_itr = std::find(m_connectionList->begin(), m_connectionList->end(), pcb);

  if (pcb_itr != m_connectionList->end()) {
    std::iter_swap(m_connectionList->begin(), pcb_itr);
    m_connectionList->erase_remaining(m_connectionList->begin() + 1, ConnectionList::disconnect_available);
  } else {
    m_connectionList->erase_remaining(m_connectionList->begin(), ConnectionList::disconnect_available);
  }

  // Switch to normal seeding.
  auto itr = manager->download_manager()->find(m_info);
  (*itr)->set_connection_type(Download::CONNECTION_SEED);
  m_connectionList->slot_new_connection(&createPeerConnectionSeed);

  m_initial_seeding.reset();

  // And close the current connection.
  throw close_connection();
}

bool
DownloadMain::want_pex_msg() {
  return m_info->is_pex_active() && m_peerList.available_list()->want_more();
}

void
DownloadMain::update_endgame() {
  if (!m_delegator.get_aggressive() &&
      file_list()->completed_chunks() + m_delegator.transfer_list()->size() + 5 >= file_list()->size_chunks())
    m_delegator.set_aggressive(true);
}

void
DownloadMain::receive_chunk_done(unsigned int index) {
  // TODO: Should we unmap the chunk here if we want sequential access?
  ChunkHandle handle = m_chunkList->get(index, ChunkList::get_hashing);

  if (!handle.is_valid())
    throw storage_error("DownloadState::chunk_done(...) called with an index we couldn't retrieve from storage");

  m_slot_hash_check_add(handle);
}

void
DownloadMain::receive_corrupt_chunk(PeerInfo* peerInfo) {
  peerInfo->set_failed_counter(peerInfo->failed_counter() + 1);

  // Just use some very primitive heuristics here to decide if we're
  // going to disconnect the peer. Also, consider adding a flag so we
  // don't recalculate these things whenever the peer reconnects.

  // That is... non at all ;)

  if (peerInfo->failed_counter() > HandshakeManager::max_failed)
    connection_list()->erase(peerInfo, ConnectionList::disconnect_unwanted);
}

void
DownloadMain::add_peer(const rak::socket_address& sa) {
  m_slot_start_handshake(sa, this);
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
         connection_list()->size() + m_slot_count_handshakes(this) < connection_list()->max_size()) {
    rak::socket_address sa = peer_list()->available_list()->pop_random();

    if (connection_list()->find(sa.c_sockaddr()) == connection_list()->end())
      m_slot_start_handshake(sa, this);
  }
}

void
DownloadMain::receive_tracker_success() {
  if (!info()->is_active())
    return;

  this_thread::scheduler()->update_wait_for_ceil_seconds(&m_task_tracker_request, 10s);
}

void
DownloadMain::receive_tracker_request() {
  if ((info()->is_pex_enabled() && info()->size_pex()) > 0
      || connection_list()->size() + peer_list()->available_list()->size() / 2 >= connection_list()->min_size()) {

    m_tracker_controller.stop_requesting();
    return;
  }

  m_tracker_controller.start_requesting();
}

static bool
SocketAddressCompact_less(const SocketAddressCompact& a, const SocketAddressCompact& b) {
  return (a.addr < b.addr) || ((a.addr == b.addr) && (a.port < b.port));
}

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
    m_info->set_flags(DownloadInfo::flag_pex_active);

    // Only set PEX_ENABLE if we don't have max_size_pex set to zero.
    if (m_info->size_pex() < m_info->max_size_pex())
      togglePex = PeerConnectionBase::PEX_ENABLE;

  } else if (m_info->is_pex_active() &&
             m_connectionList->size() >= m_connectionList->min_size()) {
//              m_peerList.available_list()->size() >= m_peerList.available_list()->max_size() / 2) {
    togglePex = PeerConnectionBase::PEX_DISABLE;
    m_info->unset_flags(DownloadInfo::flag_pex_active);
  }

  // Return if we don't really want to do anything?

  ProtocolExtension::PEXList current;

  for (auto& connection : *m_connectionList) {
    auto pcb = connection->m_ptr();
    auto sa  = rak::socket_address::cast_from(pcb->peer_info()->socket_address());

    if (pcb->peer_info()->listen_port() != 0 && sa->family() == rak::socket_address::af_inet)
      current.emplace_back(sa->sa_inet()->address_n(), htons(pcb->peer_info()->listen_port()));

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
      auto buffer = new char[message->length()];
      memcpy(buffer, message->data(), message->length());
      message->set(buffer, buffer + message->length(), true);
    }

    pcb->do_peer_exchange();
  }

  std::sort(current.begin(), current.end(), SocketAddressCompact_less);

  ProtocolExtension::PEXList added;
  added.reserve(current.size());
  std::set_difference(current.begin(), current.end(), m_ut_pex_list.begin(), m_ut_pex_list.end(), std::back_inserter(added), SocketAddressCompact_less);

  ProtocolExtension::PEXList removed;
  removed.reserve(m_ut_pex_list.size());
  std::set_difference(m_ut_pex_list.begin(), m_ut_pex_list.end(), current.begin(), current.end(), std::back_inserter(removed), SocketAddressCompact_less);

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
    m_ut_pex_list.erase(
      std::set_difference(
        m_ut_pex_list.begin(), m_ut_pex_list.end(), removed.begin(), removed.end(), m_ut_pex_list.begin(), SocketAddressCompact_less),
      m_ut_pex_list.end());
    m_ut_pex_list.insert(m_ut_pex_list.end(), added.begin(), added.end());

    std::sort(m_ut_pex_list.begin(), m_ut_pex_list.end(), SocketAddressCompact_less);

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

void
DownloadMain::set_metadata_size(size_t size) {
  if (m_info->is_meta_download()) {
	if(size == 0 || size > (1 << 26))
		throw communication_error("Peer-supplied invalid metadata size.");

    if (m_fileList.size_bytes() < 2)
      file_list()->reset_filesize(size);
    else if (size != m_fileList.size_bytes())
      throw communication_error("Peer-supplied metadata size mismatch.");

  } else if (m_info->metadata_size() && m_info->metadata_size() != size) {
      throw communication_error("Peer-supplied metadata size mismatch.");
  }

  m_info->set_metadata_size(size);
}

} // namespace torrent
