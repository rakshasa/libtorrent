#include "config.h"

#include "tracker_udp.h"

#include <cstdio>
#include <netdb.h>

#include "manager.h"
#include "net/address_list.h"
#include "torrent/connection_manager.h"
#include "torrent/net/fd.h"
#include "torrent/net/network_config.h"
#include "torrent/net/network_manager.h"
#include "torrent/net/resolver.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"
#include "torrent/utils/option_strings.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_hash(LOG_TRACKER_REQUESTS, info().info_hash, "tracker_udp", "%p : " log_fmt, static_cast<TrackerWorker*>(this), __VA_ARGS__);

#define LT_LOG_DUMP(log_dump_data, log_dump_size, log_fmt, ...)         \
  lt_log_print_hash_dump(LOG_TRACKER_DUMP, log_dump_data, log_dump_size, info().info_hash, \
                         "tracker_udp", "%p : " log_fmt, static_cast<TrackerWorker*>(this), __VA_ARGS__);

namespace torrent {

TrackerUdp::TrackerUdp(const TrackerInfo& info, int flags) :
  TrackerWorker(info, flags) {

  m_task_timeout.slot() = [this] { receive_timeout(); };
}

TrackerUdp::~TrackerUdp() {
  close_directly();
}

bool
TrackerUdp::is_busy() const {
  return m_fileDesc != -1;
}

void
TrackerUdp::send_event(tracker::TrackerState::event_enum new_state) {
  LT_LOG("sending event : state:%s url:%s", option_as_string(OPTION_TRACKER_EVENT, new_state), info().url.c_str());

  // TODO: Don't close fd for every new request.
  close_directly();

  hostname_type hostname;

  if (!parse_udp_url(info().url, hostname, m_port))
    return receive_failed("could not parse hostname or port");

  lock_and_set_latest_event(new_state);

  m_send_state = new_state;
  m_resolver_requesting = true;
  m_sending_announce = true;

  LT_LOG("resolving hostname : address:%s", hostname.data());

  // TODO: Also check failed counter....
  // TODO: Check for changes to block (NC should instead clear us on network changes)

  if ((m_inet_address == nullptr && m_inet6_address == nullptr) ||
      (this_thread::cached_time() - m_time_last_resolved) > 24h ||
      m_failed_since_last_resolved > 3) {

    int family = AF_UNSPEC;
    bool block_ipv4 = config::network_config()->is_block_ipv4();
    bool block_ipv6 = config::network_config()->is_block_ipv6();

    if (block_ipv4 && block_ipv6)
      return receive_failed("cannot send tracker event, both IPv4 and IPv6 are blocked");
    else if (block_ipv4)
      family = AF_INET6;
    else if (block_ipv6)
      family = AF_INET;

    // Currently discarding SOCK_DGRAM filter.
    this_thread::resolver()->resolve_both(static_cast<TrackerWorker*>(this), hostname.data(), family,
                                          [this](c_sin_shared_ptr sin, c_sin6_shared_ptr sin6, int err) {
                                            receive_resolved(sin, sin6, err);
                                          });
    return;
  }

  start_announce();
}

void
TrackerUdp::send_scrape() {
  throw internal_error("Tracker type UDP does not support scrape.");
}

bool
TrackerUdp::parse_udp_url(const std::string& url, hostname_type& hostname, int& port) {
  if (std::sscanf(url.c_str(), "udp://%1023[^:]:%i", hostname.data(), &port) == 2 && hostname[0] != '\0' &&
      port > 0 && port < (1 << 16))
    return true;

  if (std::sscanf(url.c_str(), "udp://[%1023[^]]]:%i", hostname.data(), &port) == 2 && hostname[0] != '\0' &&
      port > 0 && port < (1 << 16))
    return true;

  return false;
}

// TODO: Controller should not need to close the tracker when starting a new request.

void
TrackerUdp::close() {
  LT_LOG("closing event : state:%s url:%s",
         option_as_string(OPTION_TRACKER_EVENT, state().latest_event()), info().url.c_str());

  close_directly();
}

void
TrackerUdp::close_directly() {
  LT_LOG("closing directly : state:%s url:%s",
         option_as_string(OPTION_TRACKER_EVENT, state().latest_event()), info().url.c_str());

  m_slot_close();

  this_thread::resolver()->cancel(static_cast<TrackerWorker*>(this));
  this_thread::scheduler()->erase(&m_task_timeout);

  m_resolver_requesting = false;
  m_sending_announce = false;

  m_read_buffer.reset();
  m_write_buffer.reset();

  if (!get_fd().is_valid())
    return;

  this_thread::event_remove_and_close(this);

  get_fd().close();
  get_fd().clear();
}

tracker_enum
TrackerUdp::type() const {
  return TRACKER_UDP;
}

void
TrackerUdp::receive_failed(const std::string& msg) {
  m_failed_since_last_resolved++;

  close_directly();
  m_slot_failure(msg);
}

// TODO: Only resolve when we don't have a valid address, failed too many times or network change
// events.
void
TrackerUdp::receive_resolved(c_sin_shared_ptr& sin, c_sin6_shared_ptr& sin6, int err) {
  if (std::this_thread::get_id() != torrent::main_thread::thread_id())
    throw internal_error("TrackerUdp::receive_resolved() called from a different thread.");

  LT_LOG("received resolved", 0);

  if (!m_resolver_requesting)
    throw internal_error("TrackerUdp::receive_resolved() called but m_resolver_requesting is false.");

  m_resolver_requesting = false;

  if (err != 0) {
    LT_LOG("could not resolve hostname : error:'%s'", gai_strerror(err));
    return receive_failed("could not resolve hostname : error:'" + std::string(gai_strerror(err)) + "'");
  }

  if (sin != nullptr) {
    m_inet_address = sin_copy(sin.get());
    sa_set_port(reinterpret_cast<sockaddr*>(m_inet_address.get()), m_port);
  } else {
    m_inet_address = nullptr;
  }

  if (sin6 != nullptr) {
    m_inet6_address = sin6_copy(sin6.get());
    sa_set_port(reinterpret_cast<sockaddr*>(m_inet6_address.get()), m_port);
  } else {
    m_inet6_address = nullptr;
  }

  m_time_last_resolved = this_thread::cached_time();
  m_failed_since_last_resolved = 0;

  start_announce();
}

void
TrackerUdp::receive_timeout() {
  if (m_task_timeout.is_scheduled())
    throw internal_error("TrackerUdp::receive_timeout() called but m_task_timeout is still scheduled.");

  if (--m_tries == 0) {
    receive_failed("unable to connect to UDP tracker");
    return;
  }

  this_thread::scheduler()->wait_for_ceil_seconds(&m_task_timeout, std::chrono::seconds(udp_timeout));
  this_thread::event_insert_write(this);
}

void
TrackerUdp::start_announce() {
  if (!m_sending_announce)
    throw internal_error("TrackerUdp::start_announce() called but m_sending_announce is false.");

  if (m_fileDesc != -1)
    throw internal_error("TrackerUdp::start_announce() called but m_fileDesc is already open.");

  m_sending_announce = false;

  c_sa_shared_ptr bind_address;

  // TODO: This does not properly handle blocked protocols.

  if (m_inet_address != nullptr && m_inet6_address != nullptr) {
    bind_address = config::network_config()->bind_address_or_any_and_null();

    if (bind_address != nullptr) {
      if (bind_address->sa_family == AF_INET6)
        m_current_address = reinterpret_cast<sockaddr*>(m_inet6_address.get());
      else
        m_current_address = reinterpret_cast<sockaddr*>(m_inet_address.get());
    }

  } else if (m_inet_address != nullptr) {
    bind_address = config::network_config()->bind_address_for_connect(AF_INET);
    m_current_address = reinterpret_cast<sockaddr*>(m_inet_address.get());

  } else if (m_inet6_address != nullptr) {
    bind_address = config::network_config()->bind_address_for_connect(AF_INET6);
    m_current_address = reinterpret_cast<sockaddr*>(m_inet6_address.get());

  } else {
    throw internal_error("TrackerUdp::start_announce() called but both m_inet_address and m_inet6_address are nullptr.");
  }

  if (bind_address == nullptr) {
    LT_LOG("could not create outgoing connection: blocked or invalid bind address", 0);
    return receive_failed("could not create outgoing connection: blocked or invalid bind address");
  }

  if (bind_address->sa_family != m_current_address->sa_family)
    throw internal_error("TrackerUdp::start_announce() bind address family does not match connect address family.");

  // TODO: Properly handle retry with failover to other protocol, and check prefer_ipv6.

  LT_LOG("starting announce : connect_address:%s bind_address:%s",
         sa_pretty_str(m_current_address).c_str(), sa_pretty_str(bind_address.get()).c_str());

  m_fileDesc = fd_open_family(fd_flag_datagram | fd_flag_nonblock, bind_address->sa_family);

  if (m_fileDesc == -1) {
    LT_LOG("could not open UDP socket: fd:%i error:'%s'", m_fileDesc, std::strerror(errno));
    return receive_failed("could not open UDP socket: " + std::string(std::strerror(errno)));
  }

  if (!sa_is_any(bind_address.get()) && !fd_bind(m_fileDesc, bind_address.get())) {
    auto pretty_addr = sa_pretty_str(bind_address.get());
    auto error_str = std::strerror(errno);

    LT_LOG("failed to bind socket to udp address : address:%s error:'%s'", pretty_addr.c_str(), error_str);
    return receive_failed("failed to bind socket to udp address '" + pretty_addr + "' with error '" + error_str + "'");
  }

  m_read_buffer = std::make_unique<ReadBuffer>();
  m_write_buffer = std::make_unique<WriteBuffer>();

  prepare_connect_input();

  // TODO: Add socket counting.
  this_thread::event_open(this);
  this_thread::event_insert_read(this);
  this_thread::event_insert_write(this);
  this_thread::event_insert_error(this);

  m_tries = udp_tries;

  this_thread::scheduler()->wait_for_ceil_seconds(&m_task_timeout, std::chrono::seconds(udp_timeout));
}

void
TrackerUdp::event_read() {
  auto read_size = read_datagram(m_read_buffer->begin(), m_read_buffer->reserved());

  if (read_size < 0)
    return;

  m_read_buffer->reset_position();
  m_read_buffer->set_end(read_size);

  LT_LOG("received reply : size:%d", read_size);
  LT_LOG_DUMP(reinterpret_cast<const char*>(m_read_buffer->begin()), read_size, "received reply", 0);

  if (read_size < 4)
    return;

  // Do something with the content here.
  switch (m_read_buffer->read_32()) {
  case 0:
    if (m_action != 0 || !process_connect_output())
      return;

    prepare_announce_input();

    this_thread::scheduler()->update_wait_for_ceil_seconds(&m_task_timeout, std::chrono::seconds(udp_timeout));

    m_tries = udp_tries;
    this_thread::event_insert_write(this);
    return;

  case 1:
    if (m_action != 1 || !process_announce_output())
      return;

    return;

  case 3:
    if (!process_error_output())
      return;

    return;

  default:
    return;
  }
}

void
TrackerUdp::event_write() {
  if (m_write_buffer->size_end() == 0)
    throw internal_error("TrackerUdp::write() called but the write buffer is empty.");

  [[maybe_unused]] int s = write_datagram_sa(m_write_buffer->begin(), m_write_buffer->size_end(), m_current_address);

  this_thread::event_remove_write(this);
}

void
TrackerUdp::event_error() {
}

void
TrackerUdp::prepare_connect_input() {
  m_write_buffer->reset();
  m_write_buffer->write_64(m_connection_id = magic_connection_id);
  m_write_buffer->write_32(m_action = 0);
  m_write_buffer->write_32(m_transaction_id = random());

  LT_LOG_DUMP(m_write_buffer->begin(), m_write_buffer->size_end(), "prepare connect (id:%" PRIx32 ")", m_transaction_id);
}

void
TrackerUdp::prepare_announce_input() {
  m_write_buffer->reset();

  m_write_buffer->write_64(m_connection_id);
  m_write_buffer->write_32(m_action = 1);
  m_write_buffer->write_32(m_transaction_id = random());

  m_write_buffer->write_range(info().info_hash.begin(), info().info_hash.end());
  m_write_buffer->write_range(info().local_id.begin(), info().local_id.end());

  auto parameters = m_slot_parameters();

  m_write_buffer->write_64(parameters.completed_adjusted);
  m_write_buffer->write_64(parameters.download_left);
  m_write_buffer->write_64(parameters.uploaded_adjusted);
  m_write_buffer->write_32(m_send_state);

  auto local_address = config::network_config()->local_inet_address();

  if (local_address->sa_family == AF_INET)
    m_write_buffer->write_32_n(reinterpret_cast<const sockaddr_in*>(local_address.get())->sin_addr.s_addr);
  else
    m_write_buffer->write_32_n(0);

  m_write_buffer->write_32(info().key);
  m_write_buffer->write_32(parameters.numwant);
  m_write_buffer->write_16(runtime::network_manager()->listen_port_or_throw());

  if (m_write_buffer->size_end() != 98)
    throw internal_error("TrackerUdp::prepare_announce_input() ended up with the wrong size");

  LT_LOG_DUMP(m_write_buffer->begin(), m_write_buffer->size_end(),
              "prepare announce (state:%s id:%" PRIx32 " up_adj:%" PRIu64 " completed_adj:%" PRIu64 " left_adj:%" PRIu64 ")",
              option_as_string(OPTION_TRACKER_EVENT, m_send_state),
              m_transaction_id, parameters.uploaded_adjusted, parameters.completed_adjusted, parameters.download_left);
}

bool
TrackerUdp::process_connect_output() {
  if (m_read_buffer->size_end() < 16 ||
      m_read_buffer->read_32() != m_transaction_id)
    return false;

  m_connection_id = m_read_buffer->read_64();

  return true;
}

bool
TrackerUdp::process_announce_output() {
  if (m_read_buffer->size_end() < 20 ||
      m_read_buffer->read_32() != m_transaction_id)
    return false;

  {
    auto guard = lock_guard();

    state().set_normal_interval(m_read_buffer->read_32());
    state().set_min_interval(tracker::TrackerState::default_min_interval);

    state().m_scrape_incomplete = m_read_buffer->read_32(); // leechers
    state().m_scrape_complete   = m_read_buffer->read_32(); // seeders
    state().m_scrape_time_last  = this_thread::cached_seconds().count();
  }

  AddressList l;

  // TODO: This might not handle IPv4-mapped IPv6 addresses correctly.

  switch (m_current_address->sa_family) {
  case AF_INET:
    std::copy(reinterpret_cast<const SocketAddressCompact*>(m_read_buffer->position()),
              reinterpret_cast<const SocketAddressCompact*>(m_read_buffer->end() - m_read_buffer->remaining() % sizeof(SocketAddressCompact)),
              std::back_inserter(l));
    break;
  case AF_INET6:
    std::copy(reinterpret_cast<const SocketAddressCompact6*>(m_read_buffer->position()),
              reinterpret_cast<const SocketAddressCompact6*>(m_read_buffer->end() - m_read_buffer->remaining() % sizeof(SocketAddressCompact6)),
              std::back_inserter(l));
    break;
  default:
    throw internal_error("TrackerUdp::process_announce_output() m_current_address is not inet or inet6.");
  }

  // Some logic here to decided on whetever we're going to close the
  // connection or not?
  close_directly();

  m_slot_success(std::move(l));

  return true;
}

bool
TrackerUdp::process_error_output() {
  if (m_read_buffer->size_end() < 8 ||
      m_read_buffer->read_32() != m_transaction_id)
    return false;

  receive_failed("received error message: " + std::string(m_read_buffer->position(), m_read_buffer->end()));
  return true;
}

} // namespace torrent
