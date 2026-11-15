#include "config.h"

#include "dht/udp_router.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <iterator>

#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/object_static_map.h"
#include "torrent/object_stream.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/system/poll.h"
#include "torrent/system/types.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_DHT_SERVER, "udp-router : " log_fmt, __VA_ARGS__);

// TODO: Add m_connections::iterator to info so we don't need to look them up. We should be able to
// replace unordered_map with map then to reduce memory usage after initial startup dht rush.

namespace torrent::dht {

UdpRouter::UdpRouter(const HashString& id, query_func query, error_func error, inactive_func inactive,
                     write_func read_done, write_func write)
  : m_id(id), m_query(std::move(query)), m_error(std::move(error)), m_inactive(std::move(inactive)),
    m_read_done(std::move(read_done)), m_write(std::move(write)) {

  m_task_timeout.slot() = [this] { receive_timeout(); };
}

UdpRouter::~UdpRouter() {
  close();
}

void
UdpRouter::open(int family, int port) {
  if (is_open())
    throw internal_error("UdpRouter::open() called but router is already open.");

  if (family != AF_INET && family != AF_INET6)
    throw internal_error("UdpRouter::open() called with unsupported family.");

  m_thread = this_thread::thread();

  auto [inet_address, inet_device, inet6_address, inet6_device] = runtime::network_config()->bind_udp_addresses_or_null();
  auto family_address = family == AF_INET6 ? inet6_address : inet_address;
  auto device_name = family == AF_INET6 ? inet6_device : inet_device;

  if (family_address == nullptr)
    throw resource_error("no valid bind address for DHT server");

  if (family_address->sa_family != family && family_address->sa_family != AF_UNSPEC)
    throw resource_error("invalid address family for DHT server");

  auto bind_address = family_address->sa_family == AF_UNSPEC
    ? (family == AF_INET6 ? sa_make_inet6_any() : sa_make_inet_any())
    : sa_copy(family_address.get());
  auto node_address = sa_copy(bind_address.get());
  sap_set_port(bind_address, port);

  auto flags = fd_flag_datagram | fd_flag_nonblock | fd_flag_reuse_address |
    (family == AF_INET6 ? fd_flag_v6only : fd_flag_v4);
  int fd = fd_open(flags);

  if (fd == -1) {
    int err = errno;
    LT_LOG("opening router failed : open failed : family:%s errno:%s", system::sa_family_enum(family), system::errno_enum_str(err).c_str());
    throw resource_error("could not open datagram socket : " + std::string(strerror(err)));
  }

  if (!device_name.empty() && !fd_bind_to_device(fd, device_name.c_str(), family)) {
    int err = errno;
    LT_LOG("opening router failed : bind to device failed : family:%s device:%s errno:%s",
           system::sa_family_enum(family), device_name.c_str(), system::errno_enum_str(err).c_str());
    fd_close(fd);
    throw resource_error("could not bind datagram socket to device : " + std::string(strerror(err)));
  }

  if (!fd_bind(fd, bind_address.get())) {
    int err = errno;
    LT_LOG("opening router failed : bind failed : family:%s bind_address:%s errno:%s",
           system::sa_family_enum(family), sa_pretty_str(bind_address.get()).c_str(), system::errno_enum_str(err).c_str());
    fd_close(fd);
    throw resource_error("could not bind datagram socket : " + std::string(strerror(err)));
  }

  set_file_descriptor(fd);
  set_socket_address(std::move(node_address));
  set_socket_device_name(device_name);

  runtime::socket_manager()->register_event_or_throw(this, runtime::category_internal, [this]() {
      this_thread::poll()->open(this);
      this_thread::poll()->insert_read(this);
    });

  LT_LOG("opened udp router : family:%s bind_address:%s", system::sa_family_enum(family), sa_pretty_str(bind_address.get()).c_str());
}

void
UdpRouter::close() {
  if (!is_open())
    return;

  assert(m_thread == this_thread::thread());

  this_thread::scheduler()->erase(&m_task_timeout);
  m_connections.clear();
  m_write_queue.clear();
  m_timeout_queue.clear();
  m_external_write = false;

  runtime::socket_manager()->unregister_event_or_throw(this, [this]() {
      this_thread::poll()->remove_and_close(this);

      fd_close_and_clear(file_descriptor_ref());
      set_socket_address(sa_make_unspec());
    });

  // TODO: Do we just let timeout expire connections, or do we send errors?

  // TODO: Send error to all?

  LT_LOG("closed udp router", 0);
}

void
UdpRouter::connect(key_type id, const sockaddr* address, connection_params params) {
  assert(m_thread == this_thread::thread());

  if (!is_open() || address == nullptr || address->sa_family != router_family() ||
      !DhtTransaction::key_match(id, address) || sa_port(address) == 0)
    throw internal_error("UdpRouter::connect() called with invalid address.");

  if (!params.prepare || !params.process || !params.error_reply || !params.failure)
    throw internal_error("UdpRouter::connect() called without connection callbacks.");

  auto address_copy = sa_copy(address);
  auto [itr, inserted] = m_connections.try_emplace(id, connection_info{});

  if (!inserted)
    throw internal_error("UdpRouter::connect() called with existing connection ID.");

  itr->second.address     = std::move(address_copy);
  itr->second.prepare     = std::move(params.prepare);
  itr->second.process     = std::move(params.process);
  itr->second.error_reply = std::move(params.error_reply);
  itr->second.failure     = std::move(params.failure);
  itr->second.packet_sent = std::move(params.packet_sent);
}

void
UdpRouter::send(key_type id) {
  assert(m_thread == this_thread::thread());

  auto itr = m_connections.find(id);

  if (itr == m_connections.end() || itr->second.retry_count != 0 || itr->second.queue_ptr != nullptr)
    throw internal_error("UdpRouter::send() called with invalid connection ID.");

  try_write_with_queues(id, &itr->second);
}

void
UdpRouter::disconnect(key_type id) {
  assert(m_thread == this_thread::thread());

  auto itr = m_connections.find(id);

  if (itr == m_connections.end())
    throw internal_error("UdpRouter::disconnect() called with invalid connection ID.");

  disconnect_unsafe(itr);
}

void
UdpRouter::start_write() {
  assert(m_thread == this_thread::thread());
  m_external_write = true;
  this_thread::poll()->insert_write(this);
}

void
UdpRouter::stop_write() {
  assert(m_thread == this_thread::thread());
  m_external_write = false;
  if (m_write_queue.empty())
    this_thread::poll()->remove_write(this);
}

void
UdpRouter::disconnect_unsafe(connection_map::iterator itr) {
  assert(itr != m_connections.end());

  if (itr->second.queue_ptr != nullptr) {
    *itr->second.queue_ptr = nullptr;
    itr->second.queue_ptr = nullptr;
  }

  clear_timeout(&itr->second);

  m_connections.erase(itr);
}

void
UdpRouter::disconnect_failure_unsafe(connection_map::iterator itr, int errno_err, int gai_err) {
  assert(itr != m_connections.end());

  auto id = itr->first;
  auto failure_fn = std::move(itr->second.failure);
  disconnect_unsafe(itr);

  failure_fn(id, errno_err, gai_err);
}

bool
UdpRouter::try_write_with_queues(key_type id, connection_info* info) {
  clear_timeout(info);

  int err = try_write(id, info);

  if (err == EAGAIN) {
    queue_write(id, info);
    return false;
  }

  if (err != 0)
    return false;

  queue_timeout(id, info);

  return true;
}

// Returns EAGAIN for all temporary errors.

int
UdpRouter::try_write(key_type id, connection_info* info) {
  if (!is_open())
    throw internal_error("UdpRouter::try_write() called but router is not open.");

  if (info->retry_count >= 3)
    throw internal_error("UdpRouter::try_write() called but retry_count is not 0.");

  int retries{};

  while (true) {
    int err = do_write(id, info);

    if (err == 0)
      break;

    if (err == EINTR) {
      if (++retries > 5)
        return EAGAIN;

      continue;
    }

    if (err == EAGAIN || err == EWOULDBLOCK || err == EINTR || err == ENOBUFS)
      return EAGAIN;

    // To properly handle this, try_write() returning true means we don't touch the connection again.
    if (err == EHOSTUNREACH || err == ENETUNREACH || err == EADDRNOTAVAIL || err == EINVAL || err == EACCES || err == EPERM) {
      if (err == EINVAL || err == EACCES || err == EPERM)
        LT_LOG("failed to write datagram : address:%s errno:%s", sa_pretty_str(info->address.get()).c_str(), system::errno_enum_str(err).c_str());

      disconnect_failure_unsafe(m_connections.find(id), err, 0);
      return err;
    }

    LT_LOG("failed to write datagram : address:%s errno:%s", sa_pretty_str(info->address.get()).c_str(), system::errno_enum_str(err).c_str());

    // TODO: Need to be handled differently.
    throw internal_error("UdpRouter::try_write() failed to write datagram : " + system::sa_family_enum_str(socket_address()->sa_family) + " : " +
                         sa_pretty_str(info->address.get()) + " : " + system::errno_enum_str(err));
  }

  if (info->packet_sent)
    info->packet_sent(id);

  return 0;
}

int
UdpRouter::do_write(key_type id, connection_info* info) {
  assert(info->address != nullptr);

  buffer_type buffer;

  buffer.reset();
  info->prepare(id, buffer);

  if (buffer.size_end() == 0)
    throw internal_error("UdpRouter::do_write() prepare function did not write any data.");

  auto address       = info->address.get();
  auto bytes_written = write_datagram_sa(buffer.begin(), buffer.size_end(), address);

  if (bytes_written == -1) {
    if (errno == 0)
      throw internal_error("UdpRouter::do_write() write_datagram_sa() failed with errno 0");

    return errno;
  }

  if (bytes_written != buffer.size_end()) {
    LT_LOG("failed to write datagram : address:%s : only %d of %d bytes written", sa_pretty_str(address).c_str(), bytes_written, buffer.size_end());
    throw internal_error("UdpRouter::try_write() did not write entire datagram.");
  }

  return 0;
}

void
UdpRouter::clear_timeout(connection_info* info) {
  if (info->timeout_ptr == nullptr)
    return;

  *info->timeout_ptr = nullptr;
  info->timeout_ptr  = nullptr;
}

void
UdpRouter::queue_timeout(key_type id, connection_info* info) {
  if (info->timeout_ptr != nullptr)
    throw internal_error("UdpRouter::queue_timeout() called for connection that is already queued for timeout.");

  auto retry_count  = ++info->retry_count;
  auto timeout_time = this_thread::cached_seconds() + retry_count * 15s;

  auto pos = m_timeout_queue.end();
  while (pos != m_timeout_queue.begin() && std::get<1>(*std::prev(pos)) > timeout_time)
    --pos;

  bool first = pos == m_timeout_queue.begin();
  auto itr = m_timeout_queue.emplace(pos, id, timeout_time, info);
  info->timeout_ptr = &std::get<2>(*itr);

  if (first)
    this_thread::scheduler()->update_wait_until(&m_task_timeout, timeout_time);
}

void
UdpRouter::queue_write(key_type id, connection_info* info) {
  if (info->queue_ptr != nullptr)
    throw internal_error("UdpRouter::queue_write() called for connection that is already queued for writing.");

  if (m_write_queue.empty())
    this_thread::poll()->insert_write(this);

  m_write_queue.emplace_back(id, info);

  info->queue_ptr = &m_write_queue.back().second;
}

void
UdpRouter::receive_timeout() {
  auto now = this_thread::cached_seconds();

  while (!m_timeout_queue.empty()) {
    auto [id, timeout_time, info] = m_timeout_queue.front();

    if (timeout_time > now)
      break;

    if (info && info->timeout_ptr != &std::get<2>(m_timeout_queue.front()))
      throw internal_error("UdpRouter::receive_timeout() timeout queue entry does not match connection info.");

    m_timeout_queue.pop_front();

    if (info == nullptr)
      continue;

    if (info->failure == nullptr)
      throw internal_error("UdpRouter::receive_timeout() connection info failure callback is null.");

    info->timeout_ptr = nullptr;

    if (info->retry_count >= 3) {
      disconnect_failure_unsafe(m_connections.find(id), ETIMEDOUT, 0);
      continue;
    }

    if (info->queue_ptr != nullptr)
      throw internal_error("UdpRouter::receive_timeout() connection is currently queued for writing.");

    try_write_with_queues(id, info);
  }

  if (!m_timeout_queue.empty())
    this_thread::scheduler()->update_wait_until(&m_task_timeout, std::get<1>(m_timeout_queue.front()));
}

void
UdpRouter::process_datagram(buffer_type& buffer, const sockaddr* sa) {
  int type = '?';
  DhtMessage message;
  const HashString* node_id = nullptr;

  try {
    try {
      auto data = reinterpret_cast<const char*>(buffer.begin());
      static_map_read_bencode(data, data + buffer.size_end(), message);
    } catch (const bencode_error&) {
      return;
    }

    if (!message[key_t].is_raw_string())
      throw DhtError(dht_error_protocol, "No transaction ID");

    if (message[key_t].as_raw_string().size() > 20)
      throw DhtError(dht_error_protocol, "Transaction ID length too long");

    if (!message[key_y].is_raw_string())
      throw DhtError(dht_error_protocol, "No message type");

    if (message[key_y].as_raw_string().size() != 1)
      throw DhtError(dht_error_bad_method, "Unsupported message type");

    type = message[key_y].as_raw_string().data()[0];

    if (type == 'r' || type == 'q') {
      auto& id = message[type == 'q' ? key_a_id : key_r_id];

      if (!id.is_raw_string())
        throw DhtError(dht_error_protocol, "Invalid `id' value");

      auto id_string = id.as_raw_string();
      if (id_string.size() < HashString::size_data)
        throw DhtError(dht_error_protocol, "`id' value too short");

      node_id = HashString::cast_from(id_string.data());
    }

    if ((type == 'r' || type == 'e') && message[key_t].as_raw_string().size() != 1)
      throw DhtError(dht_error_protocol, "Invalid transaction ID type/length.");

    if (node_id != nullptr && *node_id == m_id)
      throw DhtError(dht_error_protocol, "Send your own ID, not mine");

    if (type == 'q') {
      m_query(*node_id, sa, message);
    } else if (type == 'r' || type == 'e') {
      auto id = static_cast<unsigned char>(message[key_t].as_raw_string().data()[0]);
      auto key = DhtTransaction::key(sa, id);
      auto itr = m_connections.find(key);

      if (itr == m_connections.end() || !sa_equal_addr(sa, itr->second.address.get()))
        return;

      if (type == 'r') {
        auto process = itr->second.process;
        process(key, *node_id, sa, message);
      } else {
        auto error_reply = itr->second.error_reply;
        error_reply(key, sa, message);
      }
    } else {
      throw DhtError(dht_error_bad_method, "Unknown message type.");
    }
  } catch (const bencode_error& e) {
    if ((type == 'r' || type == 'e') && node_id != nullptr) {
      m_inactive(*node_id, sa);
    } else {
      snprintf(message.data_end, message.data + DhtMessage::data_size - message.data_end, "Malformed packet: %s", e.what());
      m_error(message, sa, dht_error_protocol, message.data_end);
    }
  } catch (const DhtError& e) {
    if ((type == 'r' || type == 'e') && node_id != nullptr)
      m_inactive(*node_id, sa);
    else
      m_error(message, sa, e.code(), e.what());
  } catch (const network_error&) {
  }
}

void
UdpRouter::event_read() {
  for (unsigned int count = 0; count < max_read_datagrams; ++count) {
    sa_inet_union from_sa;
    buffer_type   buffer;

    buffer.reset();

    auto bytes_read = read_datagram_sa(buffer.begin(), buffer.reserved(), &from_sa.sa, sizeof(from_sa));

    if (bytes_read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;

      if (errno == EINTR)
        continue;

      LT_LOG("failed to read datagram : errno:%s", system::errno_enum_str(errno).c_str());
      throw internal_error("UdpRouter::event_read() failed to read datagram: " + system::errno_enum_str(errno));
    }

    if (bytes_read == 0)
      continue;

    buffer.set_end(bytes_read);

    if (sa_is_v4mapped(&from_sa.sa)) {
      auto unmapped = sin_from_v4mapped_in6(&from_sa.inet6);
      from_sa.inet = *unmapped;
    }

    if (from_sa.sa.sa_family == router_family())
      process_datagram(buffer, &from_sa.sa);
  }

  m_read_done();
}

void
UdpRouter::event_write() {
  if (m_external_write)
    m_write();

  while (!m_write_queue.empty()) {
    auto [id, info] = m_write_queue.front();

    if (info == nullptr) {
      m_write_queue.pop_front();
      continue;
    }

    assert(info->timeout_ptr == nullptr);

    int err = try_write(id, info);

    // EWOULDBLOCK/EINTR are returned as EAGAIN.
    if (err == EAGAIN)
      break;

    if (err != 0) {
      assert(std::get<1>(m_write_queue.front()) == nullptr);

      m_write_queue.pop_front();
      continue;
    }

    m_write_queue.pop_front();

    info->queue_ptr = nullptr;
    queue_timeout(id, info);
  }

  if (m_write_queue.empty() && !m_external_write)
    this_thread::poll()->remove_write(this);
}

void
UdpRouter::event_error() {
  LT_LOG("socket error event triggered", 0);
  throw internal_error("UdpRouter::event_error() triggered");
}

} // namespace torrent::dht
