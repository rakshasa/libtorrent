#include "config.h"

#include "handshake_manager.h"

#include "handshake.h"
#include "manager.h"
#include "peer_connection_base.h"
#include "download/download_main.h"
#include "net/proxy/proxy.h"
#include "protocol/encryption_policy.h"
#include "torrent/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/runtime/proxy_manager.h"
#include "torrent/peer/peer_info.h"
#include "torrent/peer/client_list.h"
#include "torrent/peer/connection_list.h"
#include "torrent/utils/log.h"
#include "torrent/utils/string_manip.h"

#define LT_LOG_SA(sa, log_fmt, ...)                                     \
  lt_log_print(LOG_CONNECTION_HANDSHAKE, "handshake_manager->%s: " log_fmt, sa_addr_str(sa).c_str(), __VA_ARGS__);
#define LT_LOG_SAP(sa, log_fmt, ...)                                 \
  lt_log_print(LOG_CONNECTION_HANDSHAKE, "handshake_manager->%s: " log_fmt, sap_addr_str(sa).c_str(), __VA_ARGS__);

namespace torrent {

namespace {

int  open_and_connect_socket(const sockaddr* connect_address);
bool setup_socket(int fd, int family);
bool filter_sockaddr(const sockaddr* sa);

} // namespace anonymous

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
  if (!filter_sockaddr(sa)) {
    LT_LOG_SA(sa, "rejected incoming connection: fd:%i : filtered", fd);
    fd_close(fd);
    return;
  }

  if (!fd_set_nonblock(fd))
    throw internal_error("HandshakeManager::add_incoming() fd_set_nonblocking failed : " + std::string(std::strerror(errno)));

  if (!setup_socket(fd, sa->sa_family)) {
    LT_LOG_SA(sa, "rejected incoming connection: fd:%i : setup socket failed : %s", fd, std::strerror(errno));
    fd_close(fd);
    return;
  }

  LT_LOG_SA(sa, "accepted incoming connection: fd:%i", fd);

  auto encryption_policy = EncryptionPolicy(runtime::network_config()->encryption_modes());

  if (sa_is_v4mapped(sa))
    handshake->initialize_incoming(this, fd, sa_from_v4mapped(sa).get(), encryption_policy);
  else
    handshake->initialize_incoming(this, fd, sa, encryption_policy);

  base_type::push_back(std::move(handshake));
}

void
HandshakeManager::add_outgoing(const sockaddr* sa, DownloadMain* download) {
  if (!runtime::socket_manager()->can_open_socket(runtime::category_generic) ||
      !filter_sockaddr(sa))
    return;

  auto encryption_policy = [download]() {
      if (download->info()->is_meta_download())
        return EncryptionPolicy(ENCRYPTION_MODE_DENY, ENCRYPTION_MODE_DENY);

      return EncryptionPolicy(runtime::network_config()->encryption_modes());
    }();

  if (sa_is_v4mapped(sa))
    create_outgoing(sa_from_v4mapped(sa).get(), download, encryption_policy);
  else
    create_outgoing(sa, download, encryption_policy);
}

void
HandshakeManager::create_outgoing(const sockaddr* sa, DownloadMain* download, const EncryptionPolicy& policy) {
  int connection_options = PeerList::connect_keep_handshakes;

  if (!policy.is_retrying())
    connection_options |= PeerList::connect_filter_recent;

  PeerInfo* peer_info = download->peer_list()->connected(sa, connection_options);

  if (peer_info == NULL || peer_info->failed_counter() > max_failed) {
    LT_LOG_SA(sa, "rejected outgoing connection: no peer info or too many failures", 0);
    return;
  }

  auto handshake = std::make_unique<Handshake>();

  auto connect_address = [sa, handshake = handshake.get()]() {
    auto proxy = runtime::proxy_manager()->create_proxy(sa);

    if (proxy != nullptr) {
      handshake->set_proxy(std::move(proxy));
      return sa_copy(handshake->proxy()->proxy_address());
    }

    return sa_copy(sa);
  }();

  auto open_func = [&]() {
      int fd = open_and_connect_socket(connect_address.get());

      if (fd == -1)
        return;

      LT_LOG_SA(handshake->proxy()->proxy_address(), "created outgoing connection : %i%s : %s/%s",
                fd, handshake->proxy() ? " : proxy" : "", policy.handshake_c_str(), policy.stream_c_str());

      handshake->initialize_outgoing(this, fd, sa, policy, download, peer_info);
    };

  auto cleanup_func = [&](bool) {
      if (!handshake->is_open()) {
        LT_LOG_SA(sa, "failed to create outgoing connection: open failed", 0);
        download->peer_list()->disconnected(peer_info, 0);
        return;
      }

      LT_LOG_SA(sa, "failed to create outgoing connection : socket manager triggered cleanup", 0);

      handshake->destroy_connection(false);
    };

  runtime::socket_manager()->open_event_or_cleanup(handshake.get(), runtime::category_generic, open_func, cleanup_func);

  if (!handshake->is_open())
    return;

  base_type::push_back(std::move(handshake));
}

void
HandshakeManager::receive_succeeded(Handshake* ptr) {
  if (!ptr->is_active())
    throw internal_error("HandshakeManager::receive_succeeded(...) called on an inactive handshake.");

  auto handshake = find_and_erase(ptr);
  auto download  = handshake->download();
  auto peer_type = handshake->bitfield()->is_all_set() ? "seed" : "leech";
  auto hash_str  = utils::copy_escape_html_str(handshake->peer_info()->id());

  auto error_func = [&](uint32_t reason) {
      LT_LOG_SA(handshake->peer_info()->socket_address(), "handshake dropped: type:%s id:%s reason:'%s'",
                peer_type, hash_str.c_str(), handshake_strerror(reason));
      handshake->destroy_connection();
    };

  if (!download->info()->is_active())
    return error_func(Handshake::e_handshake_inactive_download);

  if (!download->connection_list()->want_connection(handshake->peer_info(), handshake->bitfield()))
    return error_func(Handshake::e_handshake_unwanted_connection);

  auto fd        = handshake->file_descriptor();
  auto peer_info = handshake->peer_info();

  auto new_event = runtime::socket_manager()->transfer_event(handshake.get(), [&]() -> Event* {
      LT_LOG_SA(peer_info->socket_address(), "transfering handshake: type:%s id:%s", peer_type, hash_str.c_str());

      handshake->release_connection();

      return download->connection_list()->insert(peer_info, fd,
                                                 handshake->bitfield(),
                                                 handshake->encryption()->info(),
                                                 handshake->extensions());
    });

  if (new_event == nullptr) {
    fd_close(fd);

    download->peer_list()->disconnected(peer_info, 0);

    lt_log_print(LOG_CONNECTION_HANDSHAKE, "handshake_manager: duplicate peer: type:%s id:%s", peer_type, hash_str.c_str());
    return;
  }

  auto pcb = static_cast<PeerConnectionBase*>(new_event);

  manager->client_list()->retrieve_id(&peer_info->mutable_client_info(), peer_info->id());
  pcb->peer_chunks()->set_have_timer(handshake->initialized_time());

  LT_LOG_SA(peer_info->socket_address(), "handshake success: type:%s id:%s", peer_type, hash_str.c_str());

  if (handshake->unread_size() != 0) {
    if (handshake->unread_size() > PeerConnectionBase::ProtocolRead::buffer_size)
      throw internal_error("HandshakeManager::receive_succeeded(...) Unread data won't fit PCB's read buffer.");

    pcb->push_unread(handshake->unread_data(), handshake->unread_size());
    pcb->event_read();
  }
}

void
HandshakeManager::receive_failed(Handshake* ptr, int message, int error) {
  if (!ptr->is_active())
    throw internal_error("HandshakeManager::receive_failed(...) called on an inactive handshake.");

  auto handshake = find_and_erase(ptr);
  auto sa        = handshake->socket_address();

  handshake->destroy_connection();

  LT_LOG_SA(sa, "received error: message:%x %s.", message, handshake_strerror(error));

  auto policy = handshake->encryption()->policy();

  if (handshake->is_incoming())
    return;

  if (policy.retry_plaintext_handshake())
    policy = EncryptionPolicy(ENCRYPTION_MODE_DENY, policy.stream_mode());
  else if (policy.retry_encrypted_handshake())
    policy = EncryptionPolicy(ENCRYPTION_MODE_REQUIRE, policy.stream_mode());
  else
    return;

  policy.set_retrying();

  LT_LOG_SA(sa, "retrying handshake: %s/%s", policy.handshake_c_str(), policy.stream_c_str());

  create_outgoing(sa, handshake->download(), policy);
}

void
HandshakeManager::receive_timeout(Handshake* h) {
  receive_failed(h, Handshake::handshake_failed,
                 h->state() == Handshake::CONNECTING ?
                 Handshake::e_handshake_network_unreachable :
                 Handshake::e_handshake_network_timeout);
}

namespace {

int
open_and_connect_socket(const sockaddr* connect_address) {
  auto bind_address = runtime::network_config()->bind_address_for_connect(connect_address->sa_family);

  if (bind_address == nullptr) {
    LT_LOG_SA(connect_address, "could not create outgoing connection: blocked or invalid bind address", 0);
    return -1;
  }

  int fd = fd_open_family(fd_flag_stream | fd_flag_nonblock, connect_address->sa_family);

  if (fd == -1) {
    LT_LOG_SA(connect_address, "could not create outgoing connection: open stream failed : fd:%i : %s", fd, std::strerror(errno));
    return -1;
  }

  auto close_fn = [fd]() { fd_close(fd); return -1; };

  if (!setup_socket(fd, connect_address->sa_family)) {
    LT_LOG_SA(connect_address, "could not create outgoing connection: setup socket failed : fd:%i : %s", fd, std::strerror(errno));
    return close_fn();
  }

  if (!sa_is_any(bind_address.get()) && !fd_bind(fd, bind_address.get())) {
    LT_LOG_SA(connect_address, "could not create outgoing connection: bind failed : fd:%i : %s", fd, std::strerror(errno));
    return close_fn();
  }

  if (!fd_connect_with_family(fd, connect_address, bind_address->sa_family)) {
    LT_LOG_SA(connect_address, "could not create outgoing connection: connect failed : fd:%i : %s", fd, std::strerror(errno));
    return close_fn();
  }

  return fd;
}

bool
setup_socket(int fd, int family) {
  auto priority         = runtime::network_config()->priority();
  auto send_buffer_size = runtime::network_config()->send_buffer_size();
  auto recv_buffer_size = runtime::network_config()->receive_buffer_size();

  errno = 0;

  if (priority != runtime::NetworkConfig::iptos_default && !fd_set_priority(fd, family, priority))
    return false;

  if (send_buffer_size != 0 && !fd_set_send_buffer_size(fd, send_buffer_size))
    return false;

  if (recv_buffer_size != 0 && !fd_set_receive_buffer_size(fd, recv_buffer_size))
    return false;

  return true;
}

bool
filter_sockaddr(const sockaddr* sa) {
  if (runtime::network_config()->is_block_ipv4() && sa_is_inet(sa))
    return false;

  if (runtime::network_config()->is_block_ipv6() && sa_is_inet6(sa))
    return false;

  if (sa_is_v4mapped(sa)) {
    if (runtime::network_config()->is_block_ipv4in6())
      return false;
  }

  return true;
}

} // namespace anonymous

} // namespace torrent
