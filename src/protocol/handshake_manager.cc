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

#include <rak/socket_address.h>

#include "torrent/exceptions.h"
#include "torrent/error.h"
#include "download/download_info.h"
#include "download/download_main.h"
#include "torrent/connection_manager.h"
#include "torrent/peer/peer_info.h"
#include "torrent/peer/client_list.h"

#include "peer_connection_base.h"
#include "handshake.h"
#include "handshake_manager.h"

#include "manager.h"

namespace torrent {

inline void
handshake_manager_delete_handshake(Handshake* h) {
  h->deactivate_connection();
  h->destroy_connection();

  delete h;
}

HandshakeManager::size_type
HandshakeManager::size_info(DownloadMain* info) const {
  return std::count_if(base_type::begin(), base_type::end(), rak::equal(info, std::mem_fun(&Handshake::download)));
}

void
HandshakeManager::clear() {
  std::for_each(base_type::begin(), base_type::end(), std::ptr_fun(&handshake_manager_delete_handshake));
  base_type::clear();
}

void
HandshakeManager::erase(Handshake* handshake) {
  iterator itr = std::find(base_type::begin(), base_type::end(), handshake);

  if (itr == base_type::end())
    throw internal_error("HandshakeManager::erase(...) could not find handshake.");

  base_type::erase(itr);
}

struct handshake_manager_equal : std::binary_function<const rak::socket_address*, const Handshake*, bool> {
  bool operator () (const rak::socket_address* sa1, const Handshake* p2) const {
    return p2->peer_info() != NULL && *sa1 == *rak::socket_address::cast_from(p2->peer_info()->socket_address());
  }
};

bool
HandshakeManager::find(const rak::socket_address& sa) {
  return std::find_if(base_type::begin(), base_type::end(), std::bind1st(handshake_manager_equal(), &sa)) != base_type::end();
}

void
HandshakeManager::erase_download(DownloadMain* info) {
  iterator split = std::partition(base_type::begin(), base_type::end(), rak::not_equal(info, std::mem_fun(&Handshake::download)));

  std::for_each(split, base_type::end(), std::ptr_fun(&handshake_manager_delete_handshake));
  base_type::erase(split, base_type::end());
}

void
HandshakeManager::add_incoming(SocketFd fd, const rak::socket_address& sa) {
  if (!manager->connection_manager()->can_connect() ||
      !manager->connection_manager()->filter(sa.c_sockaddr()) ||
      !setup_socket(fd)) {
    fd.close();
    return;
  }

  manager->connection_manager()->signal_handshake_log().emit(sa.c_sockaddr(), ConnectionManager::handshake_incoming, e_none, NULL);
  manager->connection_manager()->inc_socket_count();

  Handshake* h = new Handshake(fd, this, manager->connection_manager()->encryption_options());
  h->initialize_incoming(sa);

  base_type::push_back(h);
}
  
void
HandshakeManager::add_outgoing(const rak::socket_address& sa, DownloadMain* download) {
  if (!manager->connection_manager()->can_connect() ||
      !manager->connection_manager()->filter(sa.c_sockaddr()))
    return;

  create_outgoing(sa, download, manager->connection_manager()->encryption_options());
}

void
HandshakeManager::create_outgoing(const rak::socket_address& sa, DownloadMain* download, int encryptionOptions) {
  PeerInfo* peerInfo = download->peer_list()->connected(sa.c_sockaddr(), PeerList::connect_keep_handshakes);

  if (peerInfo == NULL)
    return;

  SocketFd fd;
  const rak::socket_address* bindAddress = rak::socket_address::cast_from(manager->connection_manager()->bind_address());
  const rak::socket_address* connectAddress = &sa;

  if (rak::socket_address::cast_from(manager->connection_manager()->proxy_address())->is_valid()) {
    connectAddress = rak::socket_address::cast_from(manager->connection_manager()->proxy_address());
    encryptionOptions |= ConnectionManager::encryption_use_proxy;
  }

  if (!fd.open_stream() ||
      !setup_socket(fd) ||
      (bindAddress->is_bindable() && !fd.bind(*bindAddress)) ||
      !fd.connect(*connectAddress)) {

    if (fd.is_valid())
      fd.close();

    download->peer_list()->disconnected(peerInfo, 0);
    return;
  }

  int message;

  if (encryptionOptions & ConnectionManager::encryption_use_proxy)
    message = ConnectionManager::handshake_outgoing_proxy;
  else if (encryptionOptions & (ConnectionManager::encryption_try_outgoing | ConnectionManager::encryption_require))
    message = ConnectionManager::handshake_outgoing_encrypted;
  else
    message = ConnectionManager::handshake_outgoing;

  manager->connection_manager()->signal_handshake_log().emit(sa.c_sockaddr(), message, e_none, &download->info()->hash());
  manager->connection_manager()->inc_socket_count();

  Handshake* handshake = new Handshake(fd, this, encryptionOptions);
  handshake->initialize_outgoing(sa, download, peerInfo);

  base_type::push_back(handshake);
}

void
HandshakeManager::receive_succeeded(Handshake* handshake) {
  if (!handshake->is_active())
    throw internal_error("HandshakeManager::receive_succeeded(...) called on an inactive handshake.");

  erase(handshake);
  handshake->deactivate_connection();

  DownloadMain* download = handshake->download();
  PeerConnectionBase* pcb;

  if (download->info()->is_active() &&

      // We need to make libtorrent more selective in the clients it
      // connects to, and to move this somewhere else.
      (!download->file_list()->is_done() || !handshake->bitfield()->is_all_set()) &&

      (pcb = download->connection_list()->insert(handshake->peer_info(),
                                                              handshake->get_fd(),
                                                              handshake->bitfield(),
                                                              handshake->encryption()->info())) != NULL) {

    manager->client_list()->retrieve_id(&handshake->peer_info()->mutable_client_info(), handshake->peer_info()->id());
    manager->connection_manager()->signal_handshake_log().emit(handshake->peer_info()->socket_address(),
                                                               ConnectionManager::handshake_success,
                                                               e_none,
                                                               &download->info()->hash());

    if (handshake->unread_size() != 0) {
      if (handshake->unread_size() > PeerConnectionBase::ProtocolRead::buffer_size)
        throw internal_error("HandshakeManager::receive_succeeded(...) Unread data won't fit PCB's read buffer.");

      pcb->push_unread(handshake->unread_data(), handshake->unread_size());
      pcb->peer_chunks()->set_have_timer(handshake->initialized_time());

      pcb->event_read();
    }

    handshake->release_connection();

  } else {
    uint32_t reason;

    if (!download->info()->is_active())
      reason = e_handshake_inactive_download;
    else if (download->file_list()->is_done() && handshake->bitfield()->is_all_set())
      reason = e_handshake_unwanted_connection;
    else
      reason = e_handshake_duplicate;

    manager->connection_manager()->signal_handshake_log().emit(handshake->peer_info()->socket_address(),
                                                               ConnectionManager::handshake_dropped,
                                                               reason,
                                                               &download->info()->hash());
    handshake->destroy_connection();
  }

  delete handshake;
}

void
HandshakeManager::receive_failed(Handshake* handshake, int message, int error) {
  if (!handshake->is_active())
    throw internal_error("HandshakeManager::receive_failed(...) called on an inactive handshake.");

  const rak::socket_address* sa = handshake->socket_address();

  erase(handshake);
  handshake->deactivate_connection();
  handshake->destroy_connection();

  manager->connection_manager()->signal_handshake_log().emit(sa->c_sockaddr(),
                                                             message,
                                                             error,
                                                             handshake->download() != NULL ? &handshake->download()->info()->hash() : NULL);
  if (handshake->encryption()->should_retry()) {
    int retry_options = handshake->retry_options();
    DownloadMain* download = handshake->download();

    manager->connection_manager()->signal_handshake_log().emit(sa->c_sockaddr(),
                                                               retry_options & ConnectionManager::encryption_try_outgoing ?
                                                               ConnectionManager::handshake_retry_encrypted :
                                                               ConnectionManager::handshake_retry_plaintext,
                                                               e_none,
                                                               &download->info()->hash());

    create_outgoing(*sa, download, retry_options);
  }

  delete handshake;
}

void
HandshakeManager::receive_timeout(Handshake* h) {
  receive_failed(h, ConnectionManager::handshake_failed, e_handshake_network_error);
}

bool
HandshakeManager::setup_socket(SocketFd fd) {
  if (!fd.set_nonblock())
    return false;

  ConnectionManager* m = manager->connection_manager();

  if (m->priority() != ConnectionManager::iptos_default && !fd.set_priority(m->priority()))
    return false;

  if (m->send_buffer_size() != 0 && !fd.set_send_buffer_size(m->send_buffer_size()))
    return false;
    
  if (m->receive_buffer_size() != 0 && !fd.set_receive_buffer_size(m->receive_buffer_size()))
    return false;

  return true;
}

}
