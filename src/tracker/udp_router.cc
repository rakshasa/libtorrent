#include "config.h"

#include "udp_router.h"

#include <cassert>
#include <iterator>
#include <netdb.h>

#include "torrent/net/fd.h"
#include "torrent/net/resolver.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/system/callbacks.h"
#include "torrent/system/poll.h"
#include "torrent/system/system.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_subsystem(LOG_TRACKER_REQUESTS, "udp_router", log_fmt, __VA_ARGS__);
  // lt_log_print_subsystem(LOG_TRACKER_REQUESTS, "udp_router", "%p : " log_fmt, static_cast<TrackerWorker*>(this), __VA_ARGS__);

// TODO: Add m_connections::iterator to info so we don't need to look them up. We should be able to
// replace unordered_map with map then to reduce memory usage after initial startup tracker rush.

namespace torrent::tracker {

UdpRouter::UdpRouter()
  : m_resolver_callback_id(system::make_callback_id()) {

  std::random_device rd;
  std::mt19937       mt(rd());

  m_random_engine.seed(mt());

  m_task_timeout.slot() = [this] { receive_timeout(); };
}

UdpRouter::~UdpRouter() {
  // this_thread::resolver()->cancel(m_resolver_callback_id);
}

void
UdpRouter::open(int family) {
  if (is_open())
    throw internal_error("UdpRouter::open() called but router is already open.");

  if (family != AF_INET && family != AF_INET6)
    throw internal_error("UdpRouter::open() called with unsupported family.");

  m_thread = this_thread::thread();

  // TODO: Do a reopen_if_necessary() that checks if the bind address has changed.

  auto bind_address = runtime::network_config()->bind_address_for_connect(family);

  if (bind_address == nullptr) {
    LT_LOG("could not open udp router : blocked or invalid bind address : family:%s", family_str(family));
    return;
  }

  if (bind_address->sa_family != family)
    throw internal_error("UdpRouter::open() bind address family does not match requested family.");

  int fd = fd_open_family(fd_flag_datagram | fd_flag_nonblock, family);

  if (fd == -1) {
    LT_LOG("opening router failed : open failed : family:%s errno:%s", family_str(family), system::errno_enum_str(errno).c_str());
    return;
  }

  if (!fd_bind(fd, bind_address.get())) {
    LT_LOG("opening router failed : bind failed : family:%s bind_address:%s errno:%s",
           family_str(family), sa_pretty_str(bind_address.get()).c_str(), system::errno_enum_str(errno).c_str());
    fd_close(fd);
    return;
  }

  set_file_descriptor(fd);

  runtime::socket_manager()->register_event_or_throw(this, runtime::category_internal, [this]() {
      this_thread::poll()->open(this);
      this_thread::poll()->insert_read(this);
      this_thread::poll()->insert_error(this);
    });

  set_socket_address(sa_copy(bind_address.get()));

  LT_LOG("opened udp router : family:%s bind_address:%s", family_str(family), sa_pretty_str(bind_address.get()).c_str());
}

void
UdpRouter::close() {
  if (!is_open())
    return;

  assert(m_thread == this_thread::thread());

  this_thread::scheduler()->erase(&m_task_timeout);

  runtime::socket_manager()->unregister_event_or_throw(this, [this]() {
      this_thread::poll()->remove_and_close(this);

      fd_close(file_descriptor());
      set_file_descriptor(-1);
    });

  // TODO: Do we just let timeout expire connections, or do we send errors?
  // this_thread::resolver()->cancel_all_for_requester(this);

  // TODO: Send error to all?

  set_socket_address(sa_make_unspec());

  LT_LOG("closed udp router", 0);
}

void
UdpRouter::updated_network_config(int family) {
  assert(m_thread == this_thread::thread());

  auto bind_address = runtime::network_config()->bind_address_for_connect(family);

  if (bind_address == nullptr) {
    LT_LOG("closing udp router due to invalid or blocked bind address : family:%s", family_str(family));
    close();
    return;
  }

  if (bind_address->sa_family != family)
    throw internal_error("UdpRouter::updated_network_config() got bind address with wrong family.");

  if (sa_equal(bind_address.get(), socket_address()))
    return;

  LT_LOG("udp router bind address changed : old:%s new:%s", sa_pretty_str(socket_address()).c_str(), sa_pretty_str(bind_address.get()).c_str());

  close();
  open(family);
}


void
UdpRouter::connect(c_sa_shared_ptr address, connection_params params) {
  if (!is_open())
    return;

  assert(m_thread == this_thread::thread());
  assert(address != nullptr);

  if (address != nullptr && address->sa_family != router_family())
    throw internal_error("UdpRouter::connect() called with unsupported address family.");

  if (sa_port(address.get()) == 0)
    throw internal_error("UdpRouter::connect() called with address with port 0.");

  auto itr = connect_unsafe(std::move(address), params);

  if (!try_write(itr->first, &itr->second))
    queue_write(itr->first, &itr->second);
}

void
UdpRouter::connect(const std::string hostname, uint16_t port, connection_params params) {
  if (!is_open())
    return;

  assert(m_thread == this_thread::thread());
  assert(!hostname.empty());
  assert(port != 0);

  auto [sa, sa_compatible] = sa_lookup_numeric(hostname, router_family());

  if (!sa_compatible)
    return;

  if (sa != nullptr) {
    sap_set_port(sa, port);
    connect(std::move(sa), params);
    return;
  }

  auto itr = connect_unsafe(nullptr, params);

  auto fn = [this, id = itr->first, port](c_sin_shared_ptr sin, int err, c_sin6_shared_ptr sin6, int err6) {
      resolved_hostname(id, port, sin, err, sin6, err6);
    };

  this_thread::resolver()->resolve_both(m_resolver_callback_id, hostname, router_family(), std::move(fn));
}

void
UdpRouter::transfer(uint32_t id, connection_params params) {
  assert(m_thread == this_thread::thread());

  auto itr = m_connections.find(id);

  if (itr == m_connections.end())
    throw internal_error("UdpRouter::transfer_connection() called with invalid connection ID.");

  if (itr->second.address == nullptr)
    throw internal_error("UdpRouter::transfer_connection() called but connection does not have an address.");

  auto new_itr = connect_unsafe(std::move(itr->second.address), params);

  disconnect_unsafe(itr);

  if (!try_write(new_itr->first, &new_itr->second))
    queue_write(new_itr->first, &new_itr->second);
}

void
UdpRouter::disconnect(uint32_t id) {
  assert(m_thread == this_thread::thread());

  auto itr = m_connections.find(id);

  if (itr == m_connections.end())
    throw internal_error("UdpRouter::disconnect() called with invalid connection ID.");

  if (itr->second.address == nullptr) {
    assert(itr->second.queue_ptr == nullptr);

    // Indicate to resolved_hostname() that the connection has been disconnected.
    itr->second.prepare = nullptr;
    itr->second.process = nullptr;
    itr->second.failure = nullptr;
    return;
  }

  disconnect_unsafe(itr);
}

UdpRouter::connection_map::iterator
UdpRouter::connect_unsafe(c_sa_shared_ptr address, connection_params params) {
  assert(is_open());
  assert(address == nullptr || address->sa_family == router_family());
  assert(params.prepare);
  assert(params.process);
  assert(params.failure);
  assert(params.connected);

  connection_map::iterator itr;

  while (true) {
    uint32_t id = m_random_engine();

    if (id == 0)
      continue;

    auto [new_itr, inserted] = m_connections.try_emplace(id, connection_info{});

    if (inserted) {
      itr = new_itr;
      break;
    }
  }

  itr->second.address     = std::move(address);
  itr->second.prepare     = std::move(params.prepare);
  itr->second.process     = std::move(params.process);
  itr->second.failure     = std::move(params.failure);
  itr->second.packet_sent = std::move(params.packet_sent);

  params.connected(itr->first);
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
UdpRouter::resolved_hostname(uint32_t id, uint16_t port, c_sin_shared_ptr& sin, int err, c_sin6_shared_ptr& sin6, int err6) {
  auto itr = m_connections.find(id);

  if (itr == m_connections.end()) {
    LT_LOG("received hostname for unknown connection : id:%" PRIx32, id);
    return;
  }

  if (itr->second.address != nullptr)
    throw internal_error("UdpRouter::resolved_hostname() called but connection already has an address.");

  if (itr->second.failure == nullptr) {
    disconnect_unsafe(itr);
    return;
  }

  auto error_fn = [this, id, itr](int err) {
      LT_LOG("failed to resolve hostname : id:%" PRIx32 " error:%s", id, system::gai_enum_error(err));

      auto failure_fn = std::move(itr->second.failure);
      disconnect_unsafe(itr);

      failure_fn(id, 0, err);
    };

  sa_unique_ptr sa;

  switch (router_family()) {
  case AF_INET:
    if (sin == nullptr && err == 0)
      throw internal_error("UdpRouter::resolved_hostname() sin == nullptr but err == 0");

    if (err != 0)
      return error_fn(err);

    sa = sa_copy_in(sin.get());
    break;

  case AF_INET6:
    if (sin6 == nullptr && err6 == 0)
      throw internal_error("UdpRouter::resolved_hostname() sin6 == nullptr but err6 == 0");

    if (err6 != 0)
      return error_fn(err6);

    sa = sa_copy_in6(sin6.get());
    break;

  default:
    throw internal_error("UdpRouter::resolved_hostname() called but router socket address has unsupported family.");
  };

  sap_set_port(sa, port);

  itr->second.address = std::move(sa);

  if (!try_write(id, &itr->second))
    queue_write(id, &itr->second);
}


bool
UdpRouter::try_write(uint32_t id, connection_info* info) {
  if (info->retry_count >= 3)
    throw internal_error("UdpRouter::try_write() called but retry_count is not 0.");

  if (!is_open())
    throw internal_error("UdpRouter::try_write() called but router is not open.");

  clear_timeout(info);

  if (!do_write(id, info)) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
      return false;

    if (errno == EHOSTUNREACH || errno == ENETUNREACH || errno == EPERM) {
      auto failure_fn = std::move(info->failure);
      disconnect_unsafe(m_connections.find(id));

      failure_fn(id, errno, 0);
      return true;
    }

    LT_LOG("failed to write datagram : address:%s errno:%s", sa_pretty_str(info->address.get()).c_str(), system::errno_enum_str(errno).c_str());

    // TODO: Need to be handled differently.
    throw internal_error("UdpRouter::try_write() failed to write datagram : " + family_enum_str(socket_address()->sa_family) + " : " +
                         sa_pretty_str(info->address.get()) + " : " + system::errno_enum_str(errno));
  }

  queue_timeout(id, info);

  if (info->packet_sent)
    info->packet_sent(id);

  return true;
}

bool
UdpRouter::do_write(uint32_t id, connection_info* info) {
  m_buffer.reset();

  info->prepare(id, m_buffer);

  if (m_buffer.size_end() == 0)
    throw internal_error("UdpRouter::try_write() prepare function did not write any data.");

  auto address       = info->address.get();
  auto bytes_written = write_datagram_sa(m_buffer.begin(), m_buffer.size_end(), address);

  if (bytes_written == -1)
    return false;

  if (bytes_written != m_buffer.size_end()) {
    LT_LOG("failed to write datagram : address:%s : only %d of %d bytes written", sa_pretty_str(address).c_str(), bytes_written, m_buffer.size_end());
    throw internal_error("UdpRouter::try_write() did not write entire datagram.");
  }

  return true;
}

void
UdpRouter::clear_timeout(connection_info* info) {
  if (info->timeout_ptr == nullptr)
    return;

  *info->timeout_ptr = nullptr;
  info->timeout_ptr = nullptr;
}

void
UdpRouter::queue_timeout(uint32_t id, connection_info* info) {
  if (info->timeout_ptr != nullptr)
    throw internal_error("UdpRouter::queue_timeout() called for connection that is already queued for timeout.");

  if (m_timeout_queue.empty())
    this_thread::scheduler()->wait_until(&m_task_timeout, this_thread::cached_seconds() + 15s);

  m_timeout_queue.emplace_back(id, this_thread::cached_seconds() + 15s, info);

  info->timeout_ptr = &std::get<2>(m_timeout_queue.back());
  info->retry_count++;
}

void
UdpRouter::queue_write(uint32_t id, connection_info* info) {
  if (info->queue_ptr != nullptr)
    throw internal_error("UdpRouter::queue_write() called for connection that is already queued for writing.");

  if (m_write_queue.empty())
    this_thread::poll()->insert_write(this);

  m_write_queue.emplace_back(id, info);

  info->queue_ptr = &m_write_queue.back().second;
}

uint32_t
UdpRouter::peek_transaction_id(buffer_type& buffer) const {
  if (buffer.size_end() < 8)
    return 0;

  buffer.read_32();

  uint32_t transaction_id = buffer.read_32();

  buffer.reset_position();
  return transaction_id;
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

    if (info == nullptr) {
      continue;
    }

    if (info->failure == nullptr)
      throw internal_error("UdpRouter::receive_timeout() connection info failure callback is null.");

    info->timeout_ptr = nullptr;

    if (info->retry_count < 3) {
      if (!try_write(id, info))
        queue_write(id, info);

      continue;
    }

    auto failure_fn = std::move(info->failure);

    disconnect_unsafe(m_connections.find(id));
    failure_fn(id, ETIMEDOUT, 0);
  }

  if (!m_timeout_queue.empty())
    this_thread::scheduler()->update_wait_until(&m_task_timeout, std::get<1>(m_timeout_queue.front()));
}

void
UdpRouter::event_read() {
  while (true) {
    sa_inet_union from_sa;

    m_buffer.reset();

    auto bytes_read = read_datagram_sa(m_buffer.begin(), m_buffer.reserved(), &from_sa.sa, sizeof(from_sa));

    if (bytes_read == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;

      if (errno == EINTR)
        continue;

      LT_LOG("failed to read datagram : errno:%s", system::errno_enum_str(errno).c_str());
      throw internal_error("UdpRouter::event_read() failed to read datagram: " + system::errno_enum_str(errno));
    }

    if (bytes_read == 0)
      throw internal_error("UdpRouter::event_read() read datagram of length 0");

    m_buffer.set_end(bytes_read);

    uint32_t transaction_id = peek_transaction_id(m_buffer);

    if (transaction_id == 0) {
      // LT_LOG("received datagram with invalid transaction ID : address:%s", sa_pretty_str(&from_sa.sa).c_str());
      continue;
    }

    // It's quicker to do transaction-id lookup and then verify the address.
    auto itr = m_connections.find(transaction_id);

    if (itr == m_connections.end()) {
      // LT_LOG("received datagram with unknown transaction ID : address:%s transaction_id:%" PRIx32, sa_pretty_str(&from_sa.sa).c_str(), transaction_id);
      continue;
    }

    // While the id is random, the tracker can still send brute-force it so check that we're did not
    // disconnect before hostname resolved.
    if (itr->second.address == nullptr)
      continue;

    if (!sa_equal(&from_sa.sa, itr->second.address.get()))
      continue;

    if (!itr->second.process(transaction_id, m_buffer))
      disconnect_unsafe(itr);
  }
}

void
UdpRouter::event_write() {
  while (!m_write_queue.empty()) {
    auto [id, info] = m_write_queue.front();

    if (!try_write(id, info))
      break;

    info->queue_ptr = nullptr;
    m_write_queue.pop_front();
  }

  if (m_write_queue.empty())
    this_thread::poll()->remove_write(this);
}

void
UdpRouter::event_error() {
  LT_LOG("socket error event triggered", 0);
  throw internal_error("UdpRouter::event_error() triggered");
}

} // namespace torrent::tracker
