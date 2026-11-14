#include "config.h"

#include "dht/udp_router.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/object_static_map.h"
#include "torrent/object_stream.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/system/poll.h"
#include "torrent/system/types.h"
#include "torrent/utils/log.h"

#define LT_LOG_THIS(log_fmt, ...)                                       \
  lt_log_print(LOG_DHT_SERVER, "dht_server : " log_fmt, __VA_ARGS__);

namespace torrent::dht {

UdpRouter::UdpRouter(const HashString& id, query_func query, error_func error, inactive_func inactive,
                     write_func read_done, write_func write)
  : m_id(id), m_query(std::move(query)), m_error(std::move(error)), m_inactive(std::move(inactive)),
    m_read_done(std::move(read_done)), m_write(std::move(write)) {
}

UdpRouter::~UdpRouter() {
  close();
}

void
UdpRouter::open(int family, int port) {
  if (is_open())
    throw internal_error("dht::UdpRouter::open() called but router is already open.");

  if (family != AF_INET && family != AF_INET6)
    throw internal_error("dht::UdpRouter::open() called with unsupported family.");

  m_thread = this_thread::thread();

  auto [inet_address, inet_device, inet6_address, inet6_device] = runtime::network_config()->bind_udp_addresses_or_null();
  bool is_inet6 = family == AF_INET6;
  auto family_address = is_inet6 ? inet6_address : inet_address;
  auto family_device = is_inet6 ? inet6_device : inet_device;

  if (family_address == nullptr)
    throw resource_error(is_inet6 ? "no valid inet6 bind address for DHT server" : "no valid bind address for DHT server");

  sa_unique_ptr bind_address;
  std::string bind_device;

  switch (family_address->sa_family) {
  case AF_INET:
  case AF_INET6:
    if (family_address->sa_family != family)
      throw resource_error("invalid address family for DHT server");

    bind_address = sa_copy(family_address.get());
    bind_device = family_device;
    break;
  case AF_UNSPEC:
    bind_address = is_inet6 ? sa_make_inet6_any() : sa_make_inet_any();
    bind_device = family_device;
    break;
  default:
    throw resource_error("invalid address family for DHT server");
  }

  auto node_address = sa_copy(bind_address.get());
  sap_set_port(bind_address, port);

  LT_LOG_THIS("starting server : %s", sap_pretty_str(bind_address).c_str());

  fd_flags open_flags = fd_flag_datagram | fd_flag_nonblock | fd_flag_reuse_address;

  if (bind_address->sa_family == AF_INET)
    open_flags |= fd_flag_v4;
  else
    open_flags |= fd_flag_v6only;

  int fd = fd_open(open_flags);

  if (fd == -1) {
    LT_LOG_THIS("could not open datagram socket : %s", std::strerror(errno));
    throw resource_error("could not open datagram socket : " + std::string(strerror(errno)));
  }

  if (!bind_device.empty() && !fd_bind_to_device(fd, bind_device.c_str(), bind_address->sa_family)) {
    LT_LOG_THIS("could not bind datagram socket to device : %s : %s", bind_device.c_str(), std::strerror(errno));
    fd_close(fd);
    throw resource_error("could not bind datagram socket to device : " + bind_device + " : " + std::string(strerror(errno)));
  }

  if (!fd_bind(fd, bind_address.get())) {
    LT_LOG_THIS("could not bind datagram socket : %s", std::strerror(errno));
    fd_close(fd);
    throw resource_error("could not bind datagram socket : " + std::string(strerror(errno)));
  }

  set_file_descriptor(fd);
  set_socket_address(std::move(node_address));
  set_socket_device_name(bind_device);

  runtime::socket_manager()->register_event_or_throw(this, runtime::category_internal, [this]() {
      this_thread::poll()->open(this);
      this_thread::poll()->insert_read(this);
    });

  LT_LOG_THIS("opened udp router : family:%s bind_address:%s",
              system::sa_family_enum(family), sa_pretty_str(socket_address()).c_str());
}

void
UdpRouter::close() {
  if (!is_open())
    return;

  assert(m_thread == this_thread::thread());

  runtime::socket_manager()->unregister_event_or_throw(this, [this]() {
      this_thread::poll()->remove_and_close(this);
      fd_close_and_clear(file_descriptor_ref());
      set_socket_address(sa_make_unspec());
    });

  LT_LOG_THIS("closed udp router", 0);
}

void
UdpRouter::connect(key_type key, const sockaddr* address, connection_params params) {
  assert(m_thread == this_thread::thread());

  if (address == nullptr || !DhtTransaction::key_match(key, address))
    throw internal_error("dht::UdpRouter::connect() called with an invalid address.");

  if (!params.process || !params.failure)
    throw internal_error("dht::UdpRouter::connect() called without connection callbacks.");

  if (!m_connections.emplace(key, connection_info{sa_copy(address), std::move(params.process),
                                                    std::move(params.failure)}).second)
    throw internal_error("dht::UdpRouter::connect() called with an existing transaction key.");
}

void
UdpRouter::disconnect(key_type key) {
  assert(m_thread == this_thread::thread());

  if (m_connections.erase(key) != 1)
    throw internal_error("dht::UdpRouter::disconnect() called with an invalid transaction key.");
}

void
UdpRouter::start_write() {
  assert(m_thread == this_thread::thread());

  this_thread::poll()->insert_write(this);
}

void
UdpRouter::stop_write() {
  assert(m_thread == this_thread::thread());

  this_thread::poll()->remove_write(this);
}

void
UdpRouter::process_datagram(const char* buffer, int length, const sockaddr* sa) {
  int type = '?';
  DhtMessage message;
  raw_string nodeIdStr;
  const HashString* nodeId = NULL;

  try {
    // If it's not a valid bencode dictionary at all, it's probably not a DHT
    // packet at all, so we don't throw an error to prevent bounce loops.
    try {
      static_map_read_bencode(buffer, buffer + length, message);
    } catch (const bencode_error&) {
      return;
    }

    if (!message[key_t].is_raw_string())
      throw DhtError(dht_error_protocol, "No transaction ID");

    // Restrict the length of Transaction IDs. We echo them in our replies.
    if (message[key_t].as_raw_string().size() > 20)
      throw DhtError(dht_error_protocol, "Transaction ID length too long");

    if (!message[key_y].is_raw_string())
      throw DhtError(dht_error_protocol, "No message type");

    if (message[key_y].as_raw_string().size() != 1)
      throw DhtError(dht_error_bad_method, "Unsupported message type");

    type = message[key_y].as_raw_string().data()[0];

    // Queries and replies have node ID in different dictionaries.
    if (type == 'r' || type == 'q') {
      if (!message[type == 'q' ? key_a_id : key_r_id].is_raw_string())
        throw DhtError(dht_error_protocol, "Invalid `id' value");

      nodeIdStr = message[type == 'q' ? key_a_id : key_r_id].as_raw_string();

      if (nodeIdStr.size() < HashString::size_data)
        throw DhtError(dht_error_protocol, "`id' value too short");

      nodeId = HashString::cast_from(nodeIdStr.data());
    }

    // Sanity check the returned transaction ID.
    if ((type == 'r' || type == 'e') &&
        (!message[key_t].is_raw_string() || message[key_t].as_raw_string().size() != 1))
      throw DhtError(dht_error_protocol, "Invalid transaction ID type/length.");

    // Stupid broken implementations.
    if (nodeId != NULL && *nodeId == m_id)
      throw DhtError(dht_error_protocol, "Send your own ID, not mine");

    switch (type) {
      case 'q':
        m_query(*nodeId, sa, message);
        break;

      case 'r':
      case 'e': {
        auto id = static_cast<unsigned char>(message[key_t].as_raw_string().data()[0]);
        auto key = DhtTransaction::key(sa, id);
        auto itr = m_connections.find(key);

        if (itr == m_connections.end() || !sa_equal_addr(sa, itr->second.address.get()))
          break;

        if (type == 'r') {
          auto process = itr->second.process;
          process(key, *nodeId, sa, message);
        } else {
          auto failure = itr->second.failure;
          failure(key, sa, message);
        }
        break;
      }

      default:
        throw DhtError(dht_error_bad_method, "Unknown message type.");
    }

  // If node was querying us, reply with error packet, otherwise mark the node as "query failed",
  // so that if it repeatedly sends malformed replies we will drop it instead of propagating it
  // to other nodes.
  } catch (const bencode_error& e) {
    if ((type == 'r' || type == 'e') && nodeId != NULL) {
      m_inactive(*nodeId, sa);
    } else {
      snprintf(message.data_end, message.data + torrent::DhtMessage::data_size - message.data_end - 1, "Malformed packet: %s", e.what());
      message.data[torrent::DhtMessage::data_size - 1] = '\0';
      m_error(message, sa, dht_error_protocol, message.data_end);
    }

  } catch (const DhtError& e) {
    if ((type == 'r' || type == 'e') && nodeId != NULL)
      m_inactive(*nodeId, sa);
    else
      m_error(message, sa, e.code(), e.what());

  } catch (const network_error&) {
  }
}

void
UdpRouter::event_read() {
  for (unsigned int count = 0; count < max_read_datagrams; ++count) {
    char buffer[2048];
    sockaddr_in6 sa_raw{};
    sockaddr* sa = reinterpret_cast<sockaddr*>(&sa_raw);
    int32_t read = read_datagram_sa(buffer, sizeof(buffer), sa, sizeof(sa_raw));

    if (read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;

      if (errno == EINTR)
        continue;

      LT_LOG_THIS("failed to read datagram : errno:%s", system::errno_enum_str(errno).c_str());
      throw internal_error("dht::UdpRouter::event_read() failed to read datagram: " + system::errno_enum_str(errno));
    }

    if (read == 0)
      continue;

    if (sa_is_v4mapped(sa)) {
      auto sa_unmapped = sin_from_v4mapped_in6(&sa_raw);
      *reinterpret_cast<sockaddr_in*>(&sa_raw) = *sa_unmapped.get();
    }

    if (sa->sa_family == socket_address()->sa_family)
      process_datagram(buffer, read, sa);
  }

  m_read_done();
}

void
UdpRouter::event_write() {
  m_write();
}

void
UdpRouter::event_error() {
  LT_LOG_THIS("socket error event triggered", 0);
  throw internal_error("dht::UdpRouter::event_error() triggered");
}

} // namespace torrent::dht
