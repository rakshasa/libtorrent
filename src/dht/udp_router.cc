#include "config.h"

#include "dht/udp_router.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <iterator>
#include <netdb.h>

#include "dht/dht_transaction.h"
#include "torrent/object_stream.h"
#include "torrent/object_static_map.h"
#include "torrent/net/fd.h"
#include "torrent/net/resolver.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/system/callbacks.h"
#include "torrent/system/poll.h"
#include "torrent/system/types.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print(LOG_DHT_ROUTER, "udp-router : " log_fmt, __VA_ARGS__);

#define LT_LOG_DUMP(data, size, log_fmt, ...)                           \
  lt_log_print_dump(LOG_DHT_ROUTER, data, size, "udp-router : " log_fmt, __VA_ARGS__);

#define LT_LOG_DEBUG(log_fmt, ...)                                      \
  lt_log_print(LOG_DHT_ROUTER, "udp-router : " log_fmt, __VA_ARGS__);

// TODO: Add m_connections::iterator to info so we don't need to look them up. We should be able to
// replace unordered_map with map then to reduce memory usage after initial startup dht rush.

namespace torrent {

enum dht_key_keys {
  key_key_t,
  key_key_LAST,
};

struct dht_key_message : public static_map_type<dht_key_keys, key_key_LAST> {
  using base_type = static_map_type<dht_key_keys, key_key_LAST>;
};

template <>
const dht_key_message::key_list_type dht_key_message::base_type::keys = {
  { key_key_t, "t*S" }
};

}

namespace torrent::dht {

namespace {

UdpRouter::random_engine
create_random_engine() {
  std::random_device rd;
  std::seed_seq      seed{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};

  UdpRouter::random_engine engine;
  engine.seed(seed);

  return engine;
}

} // namespace

UdpRouter::UdpRouter()
  : m_random_engine(create_random_engine()),
    m_unknown_callback([](auto, auto, auto&) {}),
    m_resolver_callback_id(system::make_callback_id()) {

  m_task_timeout.slot() = [this] { receive_timeout(); };
}

UdpRouter::~UdpRouter() {
  // this_thread::resolver()->cancel(m_resolver_callback_id);
}

void
UdpRouter::open(int family, uint16_t port) {
  if (is_open())
    throw internal_error("UdpRouter::open() called but router is already open.");

  if (family != AF_INET && family != AF_INET6)
    throw internal_error("UdpRouter::open() called with unsupported family.");

  m_thread = this_thread::thread();

  auto [addr, device_name] = runtime::network_config()->bind_address_for_udp_connect(family);

  if (addr == nullptr) {
    LT_LOG("could not open udp router : blocked or invalid bind address : family:%s", system::sa_family_enum(family));
    return;
  }

  if (addr->sa_family != family)
    throw internal_error("UdpRouter::open() bind address family does not match requested family.");

  auto bind_address = sa_copy(addr.get());
  sap_set_port(bind_address, port);

  fd_flags open_flags = fd_flag_datagram | fd_flag_nonblock;

  // TODO; For DHT we require v4-mapped addresses to be disabled, while tracker requests... This
  // needs to be looked at closer.
  //
  // Both dht and tracker should disable v4-mapped addresses when dual-stack is used, while if the
  // client only has ipv6, it should still be able to connect to ipv4 nodes.
  //
  // Yet we separate the DhtServer and UdpTracker code paths for ipv4 and ipv6, so v4-mapped
  // connections would need to use the ipv6 UdpRouter(s), while the DhtServer/UdpTracker are split
  // by family.
  if (family == AF_INET6)
    open_flags |= fd_flag_v6only;

  int fd = fd_open_family(open_flags, family);

  if (fd == -1) {
    LT_LOG("opening router failed : open failed : family:%s errno:%s", system::sa_family_enum(family), system::errno_enum_str(errno).c_str());
    return;
  }

  if (!device_name.empty() && !fd_bind_to_device(fd, device_name.c_str(), family)) {
    LT_LOG("opening router failed : bind to device failed : family:%s device:%s errno:%s",
           system::sa_family_enum(family), device_name.c_str(), system::errno_enum_str(errno).c_str());
    fd_close(fd);
    return;
  }

  if (!fd_bind(fd, bind_address.get())) {
    LT_LOG("opening router failed : bind failed : family:%s bind_address:%s errno:%s",
           system::sa_family_enum(family), sap_pretty_str(bind_address).c_str(), system::errno_enum_str(errno).c_str());
    fd_close(fd);
    return;
  }

  LT_LOG("opened udp router : family:%s bind_address:%s", system::sa_family_enum(family), sap_pretty_str(bind_address).c_str());

  set_file_descriptor(fd);
  set_socket_address(std::move(bind_address));
  set_socket_device_name(device_name);

  runtime::socket_manager()->register_event_or_throw(this, runtime::category_internal, [this]() {
      this_thread::poll()->open(this);
      this_thread::poll()->insert_read(this);
    });
}

void
UdpRouter::close() {
  if (!is_open())
    return;

  assert(m_thread == this_thread::thread());

  this_thread::scheduler()->erase(&m_task_timeout);

  // Check if we're running in unittests.
  if (this_thread::resolver() != nullptr && net_thread::thread() != nullptr)
    this_thread::resolver()->cancel(m_resolver_callback_id);

  runtime::socket_manager()->unregister_event_or_throw(this, [this]() {
      this_thread::poll()->remove_and_close(this);

      fd_close_and_clear(file_descriptor_ref());
      set_socket_address(sa_make_unspec());
    });

  while (!m_connections.empty())
    disconnect_failure_unsafe(m_connections.begin(), ECANCELED, 0);

  m_write_queue.clear();
  m_timeout_queue.clear();

  LT_LOG("closed udp router", 0);
}

void
UdpRouter::updated_network_config(int family) {
  assert(m_thread == this_thread::thread());

  auto [bind_address, device_name] = runtime::network_config()->bind_address_for_udp_connect(family);

  if (bind_address == nullptr) {
    LT_LOG("closing udp router due to invalid or blocked bind address : family:%s", system::sa_family_enum(family));
    close();
    return;
  }

  if (bind_address->sa_family != family && bind_address->sa_family != AF_UNSPEC)
    throw internal_error("UdpRouter::updated_network_config() got bind address with wrong family.");

  if (sa_equal(bind_address.get(), socket_address()) && device_name == socket_device_name())
    return;

  LT_LOG("udp router bind address changed : old:%s new:%s", sa_pretty_str(socket_address()).c_str(), sa_pretty_str(bind_address.get()).c_str());

  auto port = sa_port(socket_address());

  close();

  if (port != 0)
    open(family, port);
}

void
UdpRouter::connect(key_type id, c_sa_shared_ptr address, connection_params params) {
  if (!is_open())
    return;

  assert(m_thread == this_thread::thread());
  assert(address != nullptr);

  if (address != nullptr && address->sa_family != router_family())
    throw internal_error("UdpRouter::connect() called with unsupported address family.");

  if (sa_port(address.get()) == 0)
    throw internal_error("UdpRouter::connect() called with address with port 0.");

  auto itr = connect_unsafe(id, std::move(address), params);

  try_write_with_queues(itr->first, &itr->second);
}

void
UdpRouter::disconnect(key_type id) {
  assert(m_thread == this_thread::thread());
  assert(id != 0);

  auto itr = m_connections.find(id);

  if (itr == m_connections.end())
    throw internal_error("UdpRouter::disconnect() called with invalid connection ID.");

  disconnect_and_clear_unsafe(itr);
}

void
UdpRouter::try_disconnect(key_type id) {
  assert(m_thread == this_thread::thread());
  assert(id != 0);

  auto itr = m_connections.find(id);

  if (itr == m_connections.end())
    return;

  disconnect_and_clear_unsafe(itr);
}

UdpRouter::connection_map::iterator
UdpRouter::connect_unsafe(key_type id, c_sa_shared_ptr address, connection_params params) {
  assert(is_open());
  assert(address == nullptr || address->sa_family == router_family());
  assert(params.prepare);
  // assert(params.process);
  assert(params.failure);

  auto [itr, inserted] = m_connections.try_emplace(id, connection_info{});

  if (!inserted) {
    if (itr->second.address != nullptr)
      throw internal_error("UdpRouter::connect_unsafe() called with an existing connection ID that is already connected.");

    if (itr->second.prepare != nullptr || itr->second.failure != nullptr)
      throw internal_error("UdpRouter::connect_unsafe() called with an existing connection ID that has callbacks set.");
  }

  itr->second.address     = std::move(address);
  itr->second.prepare     = std::move(params.prepare);
  itr->second.process     = std::move(params.process);
  itr->second.failure     = std::move(params.failure);
  itr->second.timeout     = std::move(params.timeout);
  itr->second.packet_sent = std::move(params.packet_sent);

  return itr;
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

  auto id         = itr->first;
  auto failure_fn = std::move(itr->second.failure);

  disconnect_unsafe(itr);

  failure_fn(id, errno_err, gai_err);
}

void
UdpRouter::disconnect_and_clear_unsafe(connection_map::iterator itr) {
  assert(itr != m_connections.end());

  if (itr->second.address == nullptr) {
    assert(itr->second.queue_ptr   == nullptr);
    assert(itr->second.timeout_ptr == nullptr);

    // Indicate to resolved_hostname() that the connection has been disconnected.
    itr->second.prepare = nullptr;
    itr->second.process = nullptr;
    itr->second.failure = nullptr;
    return;
  }

  disconnect_unsafe(itr);
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
    return EINVAL;

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

  LT_LOG_DEBUG("wrote datagram : address:%s : %d bytes", sa_pretty_str(address).c_str(), bytes_written);

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

  auto insert_pos = std::upper_bound(m_timeout_queue.begin(), m_timeout_queue.end(), timeout_time,
                                     [](auto time, const auto& entry) { return time < std::get<1>(entry); });
  bool was_empty = m_timeout_queue.empty();
  bool is_first = insert_pos == m_timeout_queue.begin();

  auto timeout_itr = m_timeout_queue.emplace(insert_pos, id, timeout_time, info);
  info->timeout_ptr = &std::get<2>(*timeout_itr);

  if (was_empty)
    this_thread::scheduler()->wait_until(&m_task_timeout, timeout_time);
  else if (is_first)
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

UdpRouter::key_type
peek_transaction_id(const sockaddr* sa, UdpRouter::buffer_type& buffer) {
  dht_key_message message;

  auto data = reinterpret_cast<const char*>(buffer.begin());

  try {
    static_map_read_bencode(data, data + buffer.size_end(), message);
  } catch (const bencode_error& e) {
    LT_LOG_DUMP(buffer.begin(), buffer.size_end(), "malformed datagram : address:%s error:%s", sa_pretty_str(sa).c_str(), e.what());
    return 0;
  }

  if (!message[key_key_t].is_raw_string()) {
    LT_LOG_DUMP(buffer.begin(), buffer.size_end(), "datagram without transaction id : address:%s", sa_pretty_str(sa).c_str());
    return 0;
  }

  auto& txid = message[key_key_t].as_raw_string();

  if (txid.size() == 0 || txid.size() > 4) {
    LT_LOG_DUMP(buffer.begin(), buffer.size_end(), "datagram with unsupported transaction id size : address:%s size:%zu",
                sa_pretty_str(sa).c_str(), txid.size());
    return 0;
  }

  return DhtTransaction::key(sa, txid.data(), txid.size());
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

    if (info->timeout != nullptr && !info->timeout(id, info->retry_count)) {
      m_connections.erase(id);
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

    key_type transaction_id = peek_transaction_id(&from_sa.sa, buffer);

    if (transaction_id == 0) {
      LT_LOG_DEBUG("received datagram with invalid transaction ID : address:%s", sa_pretty_str(&from_sa.sa).c_str());
      continue;
    }

    // It's quicker to do transaction-id lookup and then verify the address.
    auto itr = m_connections.find(transaction_id);

    if (itr == m_connections.end()) {
      LT_LOG_DEBUG("received unknown datagram : id:%" PRIx64 " bytes:%d address:%s",
                   transaction_id, bytes_read, sa_pretty_str(&from_sa.sa).c_str());

      m_unknown_callback(transaction_id, &from_sa.sa, buffer);
      continue;
    }

    // While the id is random, the dht can still send brute-force it so check that we're did not
    // disconnect before hostname resolved.
    if (itr->second.address == nullptr)
      continue;

    if (!sa_equal(&from_sa.sa, itr->second.address.get()))
      continue;

    LT_LOG_DEBUG("received datagram : id:%" PRIx64 " bytes:%d address:%s",
                 transaction_id, bytes_read, sa_pretty_str(&from_sa.sa).c_str());

    auto process = itr->second.process;

    if (process != nullptr && !process(transaction_id, buffer)) {
      auto current = m_connections.find(transaction_id);

      if (current != m_connections.end())
        disconnect_unsafe(current);
    }
  }
}

void
UdpRouter::event_write() {
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

  if (m_write_queue.empty())
    this_thread::poll()->remove_write(this);
}

void
UdpRouter::event_error() {
  LT_LOG("socket error event triggered", 0);
  throw internal_error("UdpRouter::event_error() triggered");
}

} // namespace torrent::dht
