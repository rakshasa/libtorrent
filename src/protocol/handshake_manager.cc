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
#include "torrent/net/fd.h"
#include "torrent/net/network_config.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/socket_manager.h"
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
    h->destroy_connection();
    h.reset();
  }

  base_type::clear();
}

HandshakeManager::value_type
HandshakeManager::find_and_erase(Handshake* handshake) {
  auto itr = std::find_if(base_type::begin(), base_type::end(), [handshake](auto& h) { return h.get() == handshake; });

  if (itr == base_type::end())
    throw internal_error("HandshakeManager::erase(...) could not find handshake.");

  auto tmp = std::move(*itr);
  base_type::erase(itr);

  return tmp;
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
      h->destroy_connection();
      h.reset();
    });

  base_type::erase(split, base_type::end());
}

// TODO: Replace by Handshake::prepare_incoming(fd), and close here.

void
HandshakeManager::add_incoming(std::unique_ptr<Handshake>& handshake, int fd, const sockaddr* sa) {
  if (!manager->connection_manager()->can_connect()) {
    LT_LOG_SA(sa, "rejected incoming connection: fd:%i : rejected by connection manager", fd);
    fd_close(fd);
    return;
  }

  if (!manager->connection_manager()->filter(sa)) {
    LT_LOG_SA(sa, "rejected incoming connection: fd:%i : filtered", fd);
    fd_close(fd);
    return;
  }

  if (!fd_set_nonblock(fd))
    throw internal_error("HandshakeManager::add_incoming() fd_set_nonblocking failed : " + std::string(strerror(errno)));

  if (!setup_socket(fd, sa->sa_family)) {
    LT_LOG_SA(sa, "rejected incoming connection: fd:%i : setup socket failed : %s", fd, std::strerror(errno));
    fd_close(fd);
    return;
  }

  LT_LOG_SA(sa, "accepted incoming connection: fd:%i", fd);

  manager->connection_manager()->inc_socket_count();

  if (sa_is_v4mapped(sa))
    handshake->initialize_incoming(this, fd, sa_from_v4mapped(sa).get(), config::network_config()->encryption_options());
  else
    handshake->initialize_incoming(this, fd, sa, config::network_config()->encryption_options());

  base_type::push_back(std::move(handshake));
}

void
HandshakeManager::add_outgoing(const sockaddr* sa, DownloadMain* download) {
  if (!manager->connection_manager()->can_connect() ||
      !manager->connection_manager()->filter(sa))
    return;

  auto encryption_options = config::network_config()->encryption_options();

  if (download->info()->is_meta_download()) {
    encryption_options &= ~net::NetworkConfig::encryption_try_outgoing;
    encryption_options &= ~net::NetworkConfig::encryption_require;
    encryption_options &= ~net::NetworkConfig::encryption_require_RC4;
    encryption_options &= ~net::NetworkConfig::encryption_enable_retry;
  }

  if (sa_is_v4mapped(sa))
    create_outgoing(sa_from_v4mapped(sa).get(), download, encryption_options);
  else
    create_outgoing(sa, download, encryption_options);
}

void
HandshakeManager::create_outgoing(const sockaddr* sa, DownloadMain* download, int encryption_options) {
  int connection_options = PeerList::connect_keep_handshakes;

  if (!(encryption_options & net::NetworkConfig::encryption_retrying))
    connection_options |= PeerList::connect_filter_recent;

  PeerInfo* peer_info = download->peer_list()->connected(sa, connection_options);

  if (peer_info == NULL || peer_info->failed_counter() > max_failed) {
    LT_LOG_SA(sa, "rejected outgoing connection: no peer info or too many failures", 0);
    return;
  }

  auto connect_address = sa_copy(sa);
  auto proxy_address = config::network_config()->proxy_address();

  if (proxy_address->sa_family != AF_UNSPEC) {
    connect_address = sa_copy(proxy_address.get());
    encryption_options |= net::NetworkConfig::encryption_use_proxy;
  }

  auto handshake = std::make_unique<Handshake>();

  auto open_func = [&]() {
      int fd = open_and_connect_socket(connect_address.get());

      if (fd == -1) {
        download->peer_list()->disconnected(peer_info, 0);
        return;
      }

      int message;

      if (encryption_options & net::NetworkConfig::encryption_use_proxy)
        message = ConnectionManager::handshake_outgoing_proxy;
      else if (encryption_options & (net::NetworkConfig::encryption_try_outgoing | net::NetworkConfig::encryption_require))
        message = ConnectionManager::handshake_outgoing_encrypted;
      else
        message = ConnectionManager::handshake_outgoing;

      if (proxy_address->sa_family != AF_UNSPEC) {
        LT_LOG_SA(sa, "created outgoing connection via proxy: fd:%i proxy:%s encryption:%x message:%x",
                  fd, sa_addr_str(proxy_address.get()).c_str(), encryption_options, message);
      } else {
        LT_LOG_SA(sa, "created outgoing connection: fd:%i encryption:%x message:%x", fd, encryption_options, message);
      }

      handshake->initialize_outgoing(this, fd, sa, encryption_options, download, peer_info);
    };

  auto cleanup_func = [&]() {
      if (!handshake->is_open()) {
        LT_LOG_SA(sa, "failed to create outgoing connection: open failed", 0);
        return;
      }

      LT_LOG_SA(sa, "failed to create outgoing connection : socket manager triggered cleanup", 0);
      handshake->destroy_connection(false);
    };

  runtime::socket_manager()->open_event_or_cleanup(handshake.get(), open_func, cleanup_func);

  if (!handshake->is_open()) {
    download->peer_list()->disconnected(peer_info, 0);
    return;
  }

  manager->connection_manager()->inc_socket_count();

  base_type::push_back(std::move(handshake));
}

void
HandshakeManager::receive_succeeded(Handshake* handshake) {
  if (!handshake->is_active())
    throw internal_error("HandshakeManager::receive_succeeded(...) called on an inactive handshake.");

  auto handshake_ptr = find_and_erase(handshake);
  auto download      = handshake->download();
  auto peer_type     = handshake->bitfield()->is_all_set() ? "seed" : "leech";

  auto error_func = [&](uint32_t reason) {
      LT_LOG_SA(handshake->peer_info()->socket_address(), "handshake dropped: type:%s id:%s reason:'%s'",
                peer_type, hash_string_to_html_str(handshake->peer_info()->id()).c_str(), strerror(reason));
    };

  if (!download->info()->is_active()) {
    error_func(e_handshake_inactive_download);
    handshake->destroy_connection();
    return;
  }

  if (!download->connection_list()->want_connection(handshake->peer_info(), handshake->bitfield())) {
    error_func(e_handshake_unwanted_connection);
    handshake->destroy_connection();
    return;
  }

  auto peer_info = handshake->peer_info();

  auto new_event = runtime::socket_manager()->transfer_event(handshake, [&]() -> Event* {
      auto fd        = handshake->file_descriptor();
      auto peer_info = handshake->peer_info();

      handshake->release_connection();

      auto pcb = download->connection_list()->insert(peer_info, fd,
                                                     handshake->bitfield(),
                                                     handshake->encryption()->info(),
                                                     handshake->extensions());
      if (pcb == nullptr) {
        error_func(e_handshake_duplicate);
        fd_close(fd);
        return nullptr;
      }

      return pcb;
    });

  if (new_event == nullptr) {
    LT_LOG_SA(handshake->peer_info()->socket_address(), "handshake dropped: type:%s id:%s reason:'socket transfer failed'",
              peer_type, hash_string_to_html_str(handshake->peer_info()->id()).c_str());
    return;
  }

  auto pcb = static_cast<PeerConnectionBase*>(new_event);

  manager->client_list()->retrieve_id(&peer_info->mutable_client_info(), peer_info->id());
  pcb->peer_chunks()->set_have_timer(handshake->initialized_time());

  LT_LOG_SA(peer_info->socket_address(), "handshake success: type:%s id:%s",
            peer_type, hash_string_to_html_str(peer_info->id()).c_str());

  if (handshake->unread_size() != 0) {
    if (handshake->unread_size() > PeerConnectionBase::ProtocolRead::buffer_size)
      throw internal_error("HandshakeManager::receive_succeeded(...) Unread data won't fit PCB's read buffer.");

    pcb->push_unread(handshake->unread_data(), handshake->unread_size());
    pcb->event_read();
  }
}

void
HandshakeManager::receive_failed(Handshake* handshake, int message, int error) {
  if (!handshake->is_active())
    throw internal_error("HandshakeManager::receive_failed(...) called on an inactive handshake.");

  auto sa            = handshake->socket_address();
  auto handshake_ptr = find_and_erase(handshake);

  handshake->destroy_connection();

  LT_LOG_SA(sa, "Received error: message:%x %s.", message, strerror(error));

  if (handshake->encryption()->should_retry()) {
    int retry_options = handshake->retry_options() | net::NetworkConfig::encryption_retrying;
    DownloadMain* download = handshake->download();

    LT_LOG_SA(sa, "Retrying %s.", retry_options & net::NetworkConfig::encryption_try_outgoing ? "encrypted" : "plaintext");

    create_outgoing(sa, download, retry_options);
  }
}

void
HandshakeManager::receive_timeout(Handshake* h) {
  receive_failed(h, ConnectionManager::handshake_failed,
                 h->state() == Handshake::CONNECTING ?
                 e_handshake_network_unreachable :
                 e_handshake_network_timeout);
}

int
HandshakeManager::open_and_connect_socket(const sockaddr* connect_address) {
  auto bind_address = config::network_config()->bind_address_for_connect(connect_address->sa_family);

  if (bind_address == nullptr) {
    LT_LOG_SA(connect_address, "could not create outgoing connection: blocked or invalid bind address", 0);
    return -1;
  }

  int fd = fd_open_family(fd_flag_stream | fd_flag_nonblock, connect_address->sa_family);

  if (fd == -1) {
    LT_LOG_SA(connect_address, "could not create outgoing connection: open stream failed : fd:%i : %s", fd, std::strerror(errno));
    return -1;
  }

  if (!setup_socket(fd, connect_address->sa_family)) {
    LT_LOG_SA(connect_address, "could not create outgoing connection: setup socket failed : fd:%i : %s", fd, std::strerror(errno));
    fd_close(fd);
    return -1;
  }

  if (!sa_is_any(bind_address.get()) && !fd_bind(fd, bind_address.get())) {
    LT_LOG_SA(connect_address, "could not create outgoing connection: bind failed : fd:%i : %s", fd, std::strerror(errno));
    fd_close(fd);
    return -1;
  }

  if (!fd_connect_with_family(fd, connect_address, bind_address->sa_family)) {
    LT_LOG_SA(connect_address, "could not create outgoing connection: connect failed : fd:%i : %s", fd, std::strerror(errno));
    fd_close(fd);
    return -1;
  }

  return fd;
}

bool
HandshakeManager::setup_socket(int fd, int family) {
  auto priority         = config::network_config()->priority();
  auto send_buffer_size = config::network_config()->send_buffer_size();
  auto recv_buffer_size = config::network_config()->receive_buffer_size();

  errno = 0;

  if (priority != net::NetworkConfig::iptos_default && !fd_set_priority(fd, family, priority))
    return false;

  if (send_buffer_size != 0 && !fd_set_send_buffer_size(fd, send_buffer_size))
    return false;

  if (recv_buffer_size != 0 && !fd_set_receive_buffer_size(fd, recv_buffer_size))
    return false;

  return true;
}

} // namespace torrent
