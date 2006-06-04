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

#include "torrent/connection_manager.h"
#include "../manager.h"

#include "torrent/exceptions.h"
#include "download/download_info.h"
#include "download/download_main.h"

#include "peer_info.h"
#include "peer_connection_base.h"
#include "handshake.h"
#include "handshake_manager.h"

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
  return std::count_if(Base::begin(), Base::end(), rak::equal(info, std::mem_fun(&Handshake::download)));
}

void
HandshakeManager::clear() {
  std::for_each(Base::begin(), Base::end(), std::bind1st(std::mem_fun(&HandshakeManager::delete_handshake), this));
  Base::clear();
}

void
HandshakeManager::erase(Handshake* handshake) {
  iterator itr = std::find(Base::begin(), Base::end(), handshake);

  if (itr == Base::end())
    throw internal_error("HandshakeManager::erase(...) could not find handshake.");

  Base::erase(itr);
}

struct handshake_manager_equal : std::binary_function<const rak::socket_address*, const Handshake*, bool> {
  bool operator () (const rak::socket_address* sa1, const Handshake* p2) const {
    return p2->peer_info() != NULL && *sa1 == *p2->peer_info()->socket_address();
  }
};

bool
HandshakeManager::find(const rak::socket_address& sa) {
  return std::find_if(Base::begin(), Base::end(), std::bind1st(handshake_manager_equal(), &sa)) != Base::end();
}

void
HandshakeManager::erase_info(DownloadMain* info) {
  iterator split = std::partition(Base::begin(), Base::end(), rak::not_equal(info, std::mem_fun(&Handshake::download)));

  std::for_each(split, Base::end(), std::bind1st(std::mem_fun(&HandshakeManager::delete_handshake), this));
  Base::erase(split, Base::end());
}

void
HandshakeManager::add_incoming(SocketFd fd, const rak::socket_address& sa) {
  if (!manager->connection_manager()->can_connect() ||
      !manager->connection_manager()->filter(sa.c_sockaddr()) ||
      !setup_socket(fd)) {
    fd.close();
    return;
  }

  manager->connection_manager()->inc_socket_count();

  Handshake* h = new Handshake(fd, this);
  h->initialize_incoming(sa);

  Base::push_back(h);
}
  
void
HandshakeManager::add_outgoing(const rak::socket_address& sa, DownloadMain* info) {
  if (!manager->connection_manager()->can_connect() ||
      !manager->connection_manager()->filter(sa.c_sockaddr()))
    return;

  SocketFd fd;

  if (!fd.open_stream())
    return;

  const rak::socket_address* bindAddress = rak::socket_address::cast_from(manager->connection_manager()->bind_address());

  if (!setup_socket(fd) ||
      (bindAddress->is_bindable() && !fd.bind(*bindAddress)) ||
      !fd.connect(sa)) {
    fd.close();
    return;
  }

  manager->connection_manager()->inc_socket_count();

  Handshake* h = new Handshake(fd, this);
  h->initialize_outgoing(sa, info);

  Base::push_back(h);
}

void
HandshakeManager::receive_succeeded(Handshake* h) {
  erase(h);
  h->clear();

  PeerConnectionBase* pcb;

  if (h->download()->is_active() &&

      // We need to make libtorrent more selective in the clients it
      // connects to, and to move this somewhere else.
      (!h->download()->content()->is_done() || !h->bitfield()->is_all_set()) &&

      (pcb = h->download()->connection_list()->insert(h->download(), h->peer_info(), h->get_fd(), h->bitfield())) != NULL) {

    h->download()->info()->signal_network_log().emit("Successful handshake: " + h->peer_info()->socket_address()->address_str());
    h->set_peer_info(NULL);

    post_insert(h, pcb);

  } else {
    manager->connection_manager()->dec_socket_count();
    h->get_fd().close();

    h->download()->info()->signal_network_log().emit("Successful handshake, failed: " + h->peer_info()->socket_address()->address_str());
  }

  h->set_fd(SocketFd());
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
HandshakeManager::receive_failed(Handshake* h) {
  erase(h);

  if (h->download() != NULL)
    h->download()->info()->signal_network_log().emit("Failed handshake: " + h->socket_address()->address_str());

  delete_handshake(h);
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
