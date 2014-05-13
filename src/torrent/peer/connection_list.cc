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

#include <algorithm>
#include <rak/functional.h>
#include <rak/socket_address.h>

#include "download/download_main.h"
#include "net/address_list.h"
#include "protocol/peer_connection_base.h"
#include "torrent/exceptions.h"
#include "torrent/download_info.h"
#include "torrent/download/choke_group.h"
#include "torrent/download/choke_queue.h"

#include "connection_list.h"
#include "peer.h"
#include "peer_info.h"

// When a peer is connected it should be removed from the list of
// available peers.
//
// When a peer is disconnected the torrent should rebalance the choked
// peers and connect to new ones if possible.

namespace torrent {

const int ConnectionList::disconnect_available;
const int ConnectionList::disconnect_quick;
const int ConnectionList::disconnect_unwanted;
const int ConnectionList::disconnect_delayed;

ConnectionList::ConnectionList(DownloadMain* download) :
  m_download(download), m_minSize(50), m_maxSize(100) {
}

void
ConnectionList::clear() {
  std::for_each(begin(), end(), rak::on(std::mem_fun(&Peer::m_ptr), rak::call_delete<PeerConnectionBase>()));
  base_type::clear();
  
  m_disconnectQueue.clear();
}

bool
ConnectionList::want_connection(PeerInfo* p, Bitfield* bitfield) {
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
  peerInfo->set_last_connection(cachedTime.seconds());
  peerConnection->initialize(m_download, peerInfo, fd, bitfield, encryptionInfo, extensions);

  if (!peerConnection->get_fd().is_valid()) {
    delete peerConnection;
    return NULL;
  }

  base_type::push_back(peerConnection);

  m_download->info()->change_flags(DownloadInfo::flag_accepting_new_peers, size() < m_maxSize);
  rak::slot_list_call(m_signalConnected, peerConnection);

  return peerConnection;
}

ConnectionList::iterator
ConnectionList::erase(iterator pos, int flags) {
  if (pos < begin() || pos >= end())
    throw internal_error("ConnectionList::erase(...) iterator out or range.");

  if (flags & disconnect_delayed) {
    m_disconnectQueue.push_back((*pos)->id());
    if (!m_download->delay_disconnect_peers().is_queued())
      priority_queue_insert(&taskScheduler, &m_download->delay_disconnect_peers(), cachedTime);
    return pos;
  }

  PeerConnectionBase* peerConnection = (*pos)->m_ptr();

  // The connection must be erased from the list before the signal is
  // emited otherwise some listeners might do stuff with the
  // assumption that the connection will remain in the list.
  *pos = base_type::back();
  base_type::pop_back();

  m_download->info()->change_flags(DownloadInfo::flag_accepting_new_peers, size() < m_maxSize);
  rak::slot_list_call(m_signalDisconnected, peerConnection);

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
  iterator itr = std::find(begin(), end(), peerInfo->connection());

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
  erase_remaining(std::partition(begin(), end(), rak::on(std::mem_fun(&Peer::c_ptr), std::mem_fun(&PeerConnectionBase::is_not_seeder))),
                  disconnect_unwanted);
}

void
ConnectionList::disconnect_queued() {
  for (queue_type::const_iterator itr = m_disconnectQueue.begin(), last = m_disconnectQueue.end(); itr != last; itr++) {
    ConnectionList::iterator conn_itr = find(m_disconnectQueue.back().c_str());

    if (conn_itr != end())
      erase(conn_itr, 0);
  }

  m_disconnectQueue = queue_type();
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
  return std::find_if(begin(), end(), rak::equal(*HashString::cast_from(id),
                                                 rak::on(std::mem_fun(&Peer::m_ptr), rak::on(std::mem_fun(&PeerConnectionBase::peer_info), std::mem_fun(&PeerInfo::id)))));
}

ConnectionList::iterator
ConnectionList::find(const sockaddr* sa) {
  return std::find_if(begin(), end(), rak::equal_ptr(rak::socket_address::cast_from(sa),
                                                     rak::on(std::mem_fun(&Peer::m_ptr), rak::on(std::mem_fun(&PeerConnectionBase::peer_info),
                                                                                                 std::mem_fun(&PeerInfo::socket_address)))));
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

}
