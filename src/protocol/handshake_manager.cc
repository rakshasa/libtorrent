#include "config.h"

#include "handshake_manager.h"

#include "handshake.h"
#include "manager.h"
#include "peer_connection_base.h"
#include "download/download_main.h"
#include "torrent/connection_manager.h"
#include "torrent/download_info.h"
#include "torrent/error.h"
#include "torrent/exceptions.h"
#include "torrent/net/network_config.h"
#include "torrent/net/socket_address.h"
#include "torrent/peer/peer_info.h"
#include "torrent/peer/client_list.h"
#include "torrent/peer/connection_list.h"
#include "torrent/utils/log.h"

#define LT_LOG_SA(sa, log_fmt, ...)                                     \
  lt_log_print(LOG_CONNECTION_HANDSHAKE, "handshake_manager->%s: " log_fmt, sa_addr_str(sa).c_str(), __VA_ARGS__);
#define LT_LOG_SAP(sa, log_fmt, ...)                                 \
  lt_log_print(LOG_CONNECTION_HANDSHAKE, "handshake_manager->%s: " log_fmt, sap_addr_str(sa).c_str(), __VA_ARGS__);

namespace torrent {

ProtocolExtension HandshakeManager::DefaultExtensions = ProtocolExtension::make_default();

HandshakeManager::HandshakeManager() = default;

HandshakeManager::~HandshakeManager() {
  clear();
}

HandshakeManager::size_type
HandshakeManager::size_info(DownloadMain* info) const {
  return std::count_if(base_type::begin(), base_type::end(), [info](auto& h) { return info == h->download(); });
}

void
HandshakeManager::clear() {
  for (auto& h : *this) {
    h->deactivate_connection();
    h->destroy_connection();
  }

  base_type::clear();
}

void
HandshakeManager::erase(Handshake* handshake) {
  auto itr = std::find_if(base_type::begin(), base_type::end(), [handshake](auto& h) { return h.get() == handshake; });

  if (itr == base_type::end())
    throw internal_error("HandshakeManager::erase(...) could not find handshake.");

  base_type::erase(itr);
}

bool
HandshakeManager::find(const sockaddr* sa) {
  return std::any_of(base_type::begin(), base_type::end(), [sa](auto& p2) {
      return p2->peer_info() != nullptr && sa_equal(sa, p2->peer_info()->socket_address());
    });
}

void
HandshakeManager::erase_download(DownloadMain* info) {
  auto split = std::partition(base_type::begin(), base_type::end(), [info](auto& h) { return info != h->download(); });

  std::for_each(split, base_type::end(), [](auto& h) {
      h->deactivate_connection();
      h->destroy_connection();
    });

  base_type::erase(split, base_type::end());
}

void
HandshakeManager::add_incoming(SocketFd fd, const sockaddr* sa) {
  if (!manager->connection_manager()->can_connect() ||
      !manager->connection_manager()->filter(sa) ||
      !setup_socket(fd)) {
    fd.close();
    return;
  }

  LT_LOG_SA(sa, "accepted incoming connection: fd:%i", fd.get_fd());

  manager->connection_manager()->inc_socket_count();

  auto h = std::make_unique<Handshake>(fd, this, manager->connection_manager()->encryption_options());
  h->initialize_incoming(sa);

  base_type::push_back(std::move(h));
}

void
HandshakeManager::add_outgoing(const sockaddr* sa, DownloadMain* download) {
  if (!manager->connection_manager()->can_connect() ||
      !manager->connection_manager()->filter(sa))
    return;

  create_outgoing(sa, download, manager->connection_manager()->encryption_options());
}

void
HandshakeManager::create_outgoing(const sockaddr* sa, DownloadMain* download, int encryption_options) {
  int connection_options = PeerList::connect_keep_handshakes;

  if (!(encryption_options & ConnectionManager::encryption_retrying))
    connection_options |= PeerList::connect_filter_recent;

  PeerInfo* peerInfo = download->peer_list()->connected(sa, connection_options);

  if (peerInfo == NULL || peerInfo->failed_counter() > max_failed)
    return;

  auto connect_address = sa_copy(sa);
  auto proxy_address = config::network_config()->proxy_address();

  if (proxy_address->sa_family != AF_UNSPEC) {
    connect_address = sa_copy(proxy_address.get());
    encryption_options |= ConnectionManager::encryption_use_proxy;
  }

  SocketFd fd;

  auto prepare_fd = [&fd, &connect_address]() {
      if (!fd.open_stream())
        return false;

      if (!setup_socket(fd))
        return false;

      auto bind_address = config::network_config()->bind_address();

      if (bind_address->sa_family != AF_UNSPEC && !fd.bind_sa(bind_address.get()))
        return false;

      if (!fd.connect_sa(connect_address.get()))
        return false;

      return true;
    };

  if (!prepare_fd()) {
    if (fd.is_valid())
      fd.close();

    download->peer_list()->disconnected(peerInfo, 0);
    return;
  }

  int message;

  if (encryption_options & ConnectionManager::encryption_use_proxy)
    message = ConnectionManager::handshake_outgoing_proxy;
  else if (encryption_options & (ConnectionManager::encryption_try_outgoing | ConnectionManager::encryption_require))
    message = ConnectionManager::handshake_outgoing_encrypted;
  else
    message = ConnectionManager::handshake_outgoing;

  LT_LOG_SA(sa, "created outgoing connection: fd:%i encryption:%x message:%x", fd.get_fd(), encryption_options, message);
  manager->connection_manager()->inc_socket_count();

  auto handshake = std::make_unique<Handshake>(fd, this, encryption_options);
  handshake->initialize_outgoing(sa, download, peerInfo);

  base_type::push_back(std::move(handshake));
}

void
HandshakeManager::receive_succeeded(Handshake* handshake) {
  if (!handshake->is_active())
    throw internal_error("HandshakeManager::receive_succeeded(...) called on an inactive handshake.");

  erase(handshake);
  handshake->deactivate_connection();

  DownloadMain* download = handshake->download();
  PeerConnectionBase* pcb;

  auto peer_type = handshake->bitfield()->is_all_set() ? "seed" : "leech";

  if (download->info()->is_active() &&
      download->connection_list()->want_connection(handshake->peer_info(), handshake->bitfield()) &&

      (pcb = download->connection_list()->insert(handshake->peer_info(),
                                                 handshake->get_fd(),
                                                 handshake->bitfield(),
                                                 handshake->encryption()->info(),
                                                 handshake->extensions())) != NULL) {

    manager->client_list()->retrieve_id(&handshake->peer_info()->mutable_client_info(), handshake->peer_info()->id());

    pcb->peer_chunks()->set_have_timer(handshake->initialized_time());

    LT_LOG_SA(handshake->peer_info()->socket_address(), "handshake success: type:%s id:%s",
              peer_type, hash_string_to_html_str(handshake->peer_info()->id()).c_str());

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

    LT_LOG_SA(handshake->peer_info()->socket_address(), "handshake dropped: type:%s id:%s reason:'%s'",
              peer_type, hash_string_to_html_str(handshake->peer_info()->id()).c_str(), strerror(reason));

    handshake->destroy_connection();
  }

  delete handshake;
}

void
HandshakeManager::receive_failed(Handshake* handshake, int message, int error) {
  if (!handshake->is_active())
    throw internal_error("HandshakeManager::receive_failed(...) called on an inactive handshake.");

  auto sa = sa_copy(handshake->socket_address());

  erase(handshake);

  handshake->deactivate_connection();
  handshake->destroy_connection();

  LT_LOG_SAP(sa, "Received error: message:%x %s.", message, strerror(error));

  if (handshake->encryption()->should_retry()) {
    int retry_options = handshake->retry_options() | ConnectionManager::encryption_retrying;
    DownloadMain* download = handshake->download();

    LT_LOG_SAP(sa, "Retrying %s.",
              retry_options & ConnectionManager::encryption_try_outgoing ? "encrypted" : "plaintext");

    create_outgoing(sa.get(), download, retry_options);
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

} // namespace torrent
