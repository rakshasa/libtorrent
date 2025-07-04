#include "config.h"

#include "torrent/peer/connection_list.h"

#include <algorithm>
#include <rak/socket_address.h>

#include "download/download_main.h"
#include "net/address_list.h"
#include "protocol/peer_connection_base.h"
#include "torrent/exceptions.h"
#include "torrent/download_info.h"
#include "torrent/download/choke_group.h"
#include "torrent/download/choke_queue.h"
#include "torrent/peer/peer.h"
#include "torrent/peer/peer_info.h"
#include "utils/functional.h"

// When a peer is connected it should be removed from the list of
// available peers.
//
// When a peer is disconnected the torrent should rebalance the choked
// peers and connect to new ones if possible.

namespace torrent {

ConnectionList::ConnectionList(DownloadMain* download) :
    m_download(download) {
}

void
ConnectionList::clear() {
  for (const auto& peer : *this) {
    delete peer->m_ptr();
  }
  base_type::clear();

  m_disconnectQueue.clear();
}

bool
ConnectionList::want_connection([[maybe_unused]] PeerInfo* p, Bitfield* bitfield) {
  if (m_download->file_list()->is_done() || m_download->initial_seeding() != NULL)
    return !bitfield->is_all_set();

  if (!m_download->info()->is_accepting_seeders())
    return !bitfield->is_all_set();

  return true;
}

PeerConnectionBase*
ConnectionList::insert(PeerInfo* peerInfo, const SocketFd& fd, Bitfield* bitfield, EncryptionInfo* encryptionInfo, ProtocolExtension* extensions) {
  if (size() >= m_maxSize)
    return NULL;

  PeerConnectionBase* peerConnection = m_slotNewConnection(encryptionInfo->is_encrypted());

  if (peerConnection == NULL || bitfield == NULL)
    throw internal_error("ConnectionList::insert(...) received a NULL pointer.");

  peerInfo->set_connection(peerConnection);
  peerInfo->set_last_connection(this_thread::cached_seconds().count());
  peerConnection->initialize(m_download, peerInfo, fd, bitfield, encryptionInfo, extensions);

  if (!peerConnection->get_fd().is_valid()) {
    delete peerConnection;
    return NULL;
  }

  base_type::push_back(peerConnection);

  m_download->info()->change_flags(DownloadInfo::flag_accepting_new_peers, size() < m_maxSize);

  ::utils::slot_list_call(m_signalConnected, peerConnection);

  return peerConnection;
}

ConnectionList::iterator
ConnectionList::erase(iterator pos, int flags) {
  if (pos < begin() || pos >= end())
    throw internal_error("ConnectionList::erase(...) iterator out or range.");

  if (flags & disconnect_delayed) {
    m_disconnectQueue.push_back((*pos)->id());

    this_thread::scheduler()->update_wait_for(&m_download->delay_disconnect_peers(), 0us);
    return pos;
  }

  PeerConnectionBase* peerConnection = (*pos)->m_ptr();

  // The connection must be erased from the list before the signal is
  // emited otherwise some listeners might do stuff with the
  // assumption that the connection will remain in the list.
  *pos = base_type::back();
  base_type::pop_back();

  m_download->info()->change_flags(DownloadInfo::flag_accepting_new_peers, size() < m_maxSize);

  ::utils::slot_list_call(m_signalDisconnected, peerConnection);

  // Before of after the signal?
  peerConnection->cleanup();
  peerConnection->mutable_peer_info()->set_connection(NULL);

  m_download->peer_list()->disconnected(peerConnection->mutable_peer_info(), PeerList::disconnect_set_time);

  // Delete after the signal to ensure the address of 'v' doesn't get
  // allocated for a different PCB in the signal.
  delete peerConnection;
  return pos;
}

void
ConnectionList::erase(Peer* p, int flags) {
  erase(std::find(begin(), end(), p), flags);
}

void
ConnectionList::erase(PeerInfo* peerInfo, int flags) {
  auto itr = std::find(begin(), end(), peerInfo->connection());

  if (itr == end())
    return;

  erase(itr, flags);
}

void
ConnectionList::erase_remaining(iterator pos, int flags) {
  flags |= disconnect_quick;

  // Need to do it one connection at the time to ensure that when the
  // signal is emited everything is in a valid state.
  while (pos != end())
    erase(--end(), flags);

  m_download->info()->change_flags(DownloadInfo::flag_accepting_new_peers, size() < m_maxSize);
}

void
ConnectionList::erase_seeders() {
  erase_remaining(std::partition(begin(), end(), [](Peer* p) { return p->c_ptr()->is_not_seeder(); }),
                  disconnect_unwanted);
}

void
ConnectionList::disconnect_queued() {
  for (const auto& queue : m_disconnectQueue) {
    auto conn_itr = find(queue.c_str());

    if (conn_itr != end())
      erase(conn_itr, 0);
  }

  m_disconnectQueue.clear();
}

struct connection_list_less {
  bool operator () (const Peer* p1, const Peer* p2) const {
    return
      *rak::socket_address::cast_from(p1->peer_info()->socket_address()) <
      *rak::socket_address::cast_from(p2->peer_info()->socket_address());
  }

  bool operator () (const rak::socket_address& sa1, const Peer* p2) const {
    return sa1 < *rak::socket_address::cast_from(p2->peer_info()->socket_address());
  }

  bool operator () (const Peer* p1, const rak::socket_address& sa2) const {
    return *rak::socket_address::cast_from(p1->peer_info()->socket_address()) < sa2;
  }
};

ConnectionList::iterator
ConnectionList::find(const char* id) {
  return std::find_if(begin(), end(), [id](Peer* p) {
    return *HashString::cast_from(id) == p->m_ptr()->peer_info()->id();
  });
}

ConnectionList::iterator
ConnectionList::find(const sockaddr* sa) {
  return std::find_if(begin(), end(), [sa](Peer* p) {
    return *rak::socket_address::cast_from(sa) == *p->m_ptr()->peer_info()->socket_address();
  });
}

void
ConnectionList::set_difference(AddressList* l) {
  std::sort(begin(), end(), connection_list_less());

  l->erase(std::set_difference(l->begin(), l->end(), begin(), end(), l->begin(), connection_list_less()),
           l->end());
}

void
ConnectionList::set_min_size(size_type v) { 
  if (v > (1 << 16))
    throw input_error("Min peer connections must be between 0 and 2^16.");

  m_minSize = v;
}

void
ConnectionList::set_max_size(size_type v) { 
  if (v > (1 << 16))
    throw input_error("Max peer connections must be between 0 and 2^16.");

  if (v == 0)
    v = choke_queue::unlimited;

  m_maxSize = v;
  m_download->info()->change_flags(DownloadInfo::flag_accepting_new_peers, size() < m_maxSize);
  //m_download->choke_group()->up_queue()->balance();
}

} // namespace torrent
