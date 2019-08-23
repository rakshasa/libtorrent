#include "config.h"

#include <rak/socket_address.h>

#include "torrent/exceptions.h"
#include "torrent/error.h"
#include "download/download_main.h"
#include "torrent/connection_manager.h"
#include "torrent/download_info.h"
#include "torrent/peer/peer_info.h"
#include "torrent/peer/client_list.h"
#include "torrent/peer/connection_list.h"
#include "torrent/utils/log.h"

#include "peer_connection_base.h"
#include "handshake.h"
#include "handshake_manager.h"

#include "manager.h"

#define LT_LOG_SA(sa, log_fmt, ...)                                     \
  lt_log_print(LOG_CONNECTION_HANDSHAKE, "handshake_manager->%s: " log_fmt, (sa)->address_str().c_str(), __VA_ARGS__);
#define LT_LOG_SA_C(sa, log_fmt, ...)                                   \
  lt_log_print(LOG_CONNECTION_HANDSHAKE, "handshake_manager->%s: " log_fmt, \
               reinterpret_cast<const rak::socket_address*>(sa)->address_str().c_str(), __VA_ARGS__);

namespace torrent {

ProtocolExtension HandshakeManager::DefaultExtensions = ProtocolExtension::make_default();

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
  iterator split = std::partition(base_type::begin(), base_type::end(),
                                  rak::not_equal(info, std::mem_fun(&Handshake::download)));

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

  LT_LOG_SA(&sa, "Adding incoming connection: fd:%i.", fd.get_fd());

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
  int connection_options = PeerList::connect_keep_handshakes;

  if (!(encryptionOptions & ConnectionManager::encryption_retrying))
    connection_options |= PeerList::connect_filter_recent;

  PeerInfo* peerInfo = download->peer_list()->connected(sa.c_sockaddr(), connection_options);

  if (peerInfo == NULL || peerInfo->failed_counter() > max_failed)
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

  LT_LOG_SA(&sa, "Adding outcoming connection: encryption:%x message:%x.", encryptionOptions, message);
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
      download->connection_list()->want_connection(handshake->peer_info(), handshake->bitfield()) &&

      (pcb = download->connection_list()->insert(handshake->peer_info(),
                                                 handshake->get_fd(),
                                                 handshake->bitfield(),
                                                 handshake->encryption()->info(),
                                                 handshake->extensions())) != NULL) {
    
    manager->client_list()->retrieve_id(&handshake->peer_info()->mutable_client_info(), handshake->peer_info()->id());
    LT_LOG_SA_C(handshake->peer_info()->socket_address(), "Handshake success.", 0);

    pcb->peer_chunks()->set_have_timer(handshake->initialized_time());

    if (handshake->unread_size() != 0) {
      if (handshake->unread_size() > PeerConnectionBase::ProtocolRead::buffer_size)
        throw internal_error("HandshakeManager::receive_succeeded(...) Unread data won't fit PCB's read buffer.");

      pcb->push_unread(handshake->unread_data(), handshake->unread_size());
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

    LT_LOG_SA_C(handshake->peer_info()->socket_address(), "Handshake dropped: %s.", strerror(reason));
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

  LT_LOG_SA(sa, "Received error: message:%x %s.", message, strerror(error));

  if (handshake->encryption()->should_retry()) {
    int retry_options = handshake->retry_options() | ConnectionManager::encryption_retrying;
    DownloadMain* download = handshake->download();

    LT_LOG_SA(sa, "Retrying %s.",
              retry_options & ConnectionManager::encryption_try_outgoing ? "encrypted" : "plaintext");

    create_outgoing(*sa, download, retry_options);
  }

  delete handshake;
}

void
HandshakeManager::receive_timeout(Handshake* h) {
  receive_failed(h, ConnectionManager::handshake_failed,
                 h->state() == Handshake::CONNECTING ?
                 e_handshake_network_unreachable :
                 e_handshake_network_timeout);
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
