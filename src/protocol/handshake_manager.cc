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
#include "torrent/peer_info.h"

#include "peer_connection_base.h"
#include "handshake.h"
#include "handshake_manager.h"

#include "manager.h"

namespace torrent {

inline void
HandshakeManager::delete_handshake(Handshake* h) {
  manager->connection_manager()->dec_socket_count();

  h->clear();
  h->get_fd().close();
  h->get_fd().clear();

  delete h;
}

HandshakeManager::size_type
HandshakeManager::size_info(DownloadMain* info) const {
  return std::count_if(base_type::begin(), base_type::end(), rak::equal(info, std::mem_fun(&Handshake::download)));
}

void
HandshakeManager::clear() {
  std::for_each(base_type::begin(), base_type::end(), std::bind1st(std::mem_fun(&HandshakeManager::delete_handshake), this));
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

  std::for_each(split, base_type::end(), std::bind1st(std::mem_fun(&HandshakeManager::delete_handshake), this));
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

  manager->connection_manager()->signal_handshake_log().emit(sa.c_sockaddr(), ConnectionManager::handshake_incoming, EH_None, "");

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
HandshakeManager::create_outgoing(const rak::socket_address& sa, DownloadMain* download, int encryption_options) {
  PeerInfo* peerInfo = download->peer_list()->connected(sa.c_sockaddr(), PeerList::connect_keep_handshakes);

  if (peerInfo == NULL)
    return;

  SocketFd fd;
  const rak::socket_address* bindAddress = rak::socket_address::cast_from(manager->connection_manager()->bind_address());

  if (!fd.open_stream() ||
      !setup_socket(fd) ||
      (bindAddress->is_bindable() && !fd.bind(*bindAddress)) ||
      !fd.connect(sa)) {

    if (fd.is_valid())
      fd.close();

    download->peer_list()->disconnected(peerInfo, 0);
    return;
  }

  if ((encryption_options & (ConnectionManager::encryption_try_outgoing | ConnectionManager::encryption_require)) != 0)
    manager->connection_manager()->signal_handshake_log().emit(sa.c_sockaddr(), ConnectionManager::handshake_outgoing_encrypted, EH_None, download->info()->hash());
  else
    manager->connection_manager()->signal_handshake_log().emit(sa.c_sockaddr(), ConnectionManager::handshake_outgoing, EH_None, download->info()->hash());

  manager->connection_manager()->inc_socket_count();

  Handshake* h = new Handshake(fd, this, encryption_options);
  h->initialize_outgoing(sa, download, peerInfo);

  base_type::push_back(h);
}

void
HandshakeManager::receive_succeeded(Handshake* h) {
  if (!h->is_active())
    throw internal_error("HandshakeManager::receive_succeeded(...) called on an inactive handshake.");

  erase(h);
  h->clear();
  h->peer_info()->unset_flags(PeerInfo::flag_handshake);

  PeerConnectionBase* pcb;

  if (h->download()->info()->is_active() &&

      // We need to make libtorrent more selective in the clients it
      // connects to, and to move this somewhere else.
      (!h->download()->content()->is_done() || !h->bitfield()->is_all_set()) &&

      (pcb = h->download()->connection_list()->insert(h->peer_info(), h->get_fd(), h->bitfield(), h->encryption_info())) != NULL) {

    manager->connection_manager()->signal_handshake_log().emit(h->peer_info()->socket_address(), ConnectionManager::handshake_success, EH_None, h->download()->info()->hash());

    h->set_peer_info(NULL);

    post_insert(h, pcb);

  } else {
    manager->connection_manager()->dec_socket_count();
    h->get_fd().close();

    uint32_t reason = EH_Duplicate;
    if (!h->download()->info()->is_active()) reason = EH_Inactive;
    if (h->download()->content()->is_done() && h->bitfield()->is_all_set()) reason = EH_SeederRejected;

    manager->connection_manager()->signal_handshake_log().emit(h->peer_info()->socket_address(), ConnectionManager::handshake_dropped, reason, h->download()->info()->hash());
  }

  h->get_fd().clear();
  delete h;
}

inline void
HandshakeManager::post_insert(Handshake* h, PeerConnectionBase* pcb) {
  if (h->unread_size() != 0) {
    pcb->push_unread(h->unread_data(), h->unread_size());
    pcb->event_read();
  }
}

void
HandshakeManager::receive_failed(Handshake* h, ConnectionManager::HandshakeMessage message, uint32_t err) {
  if (!h->is_active())
    throw internal_error("HandshakeManager::receive_failed(...) called on an inactive handshake.");

  manager->connection_manager()->signal_handshake_log().emit(h->socket_address()->c_sockaddr(), message, err, h->download() != NULL ? h->download()->info()->hash() : "");

  erase(h);

  if (h->do_retry()) {
    int retry_options = h->retry_options();
    const rak::socket_address* sa = h->socket_address();
    DownloadMain* download = h->download();

    if (retry_options & ConnectionManager::encryption_try_outgoing)
      manager->connection_manager()->signal_handshake_log().emit(h->socket_address()->c_sockaddr(), ConnectionManager::handshake_retry_encrypted, EH_None, download->info()->hash());
    else
      manager->connection_manager()->signal_handshake_log().emit(h->socket_address()->c_sockaddr(), ConnectionManager::handshake_retry_plaintext, EH_None, download->info()->hash());

    delete_handshake(h);
    create_outgoing(*sa, download, retry_options);
  } else {
    delete_handshake(h);
  }
}

void
HandshakeManager::receive_timeout(Handshake* h) {
  receive_failed(h, ConnectionManager::handshake_failed, EH_NetworkError);
}

bool
HandshakeManager::setup_socket(SocketFd fd) {
  if (!fd.set_nonblock())
    return false;

  ConnectionManager* m = manager->connection_manager();

  if (m->priority() != ConnectionManager::iptos_default && !fd.set_priority(ConnectionManager::iptos_throughput))
    return false;

  if (m->send_buffer_size() != 0 && !fd.set_send_buffer_size(m->send_buffer_size()))
    return false;
    
  if (m->receive_buffer_size() != 0 && !fd.set_receive_buffer_size(m->receive_buffer_size()))
    return false;

  return true;
}

}
