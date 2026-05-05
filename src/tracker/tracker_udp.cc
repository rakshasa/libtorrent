#include "config.h"

#include "tracker/tracker_udp.h"

#include <cstdio>
#include <netdb.h>

#include "manager.h"
#include "net/address_list.h"
#include "torrent/connection_manager.h"
#include "torrent/net/fd.h"
#include "torrent/net/resolver.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/utils/log.h"
#include "torrent/utils/option_strings.h"
#include "tracker/thread_tracker.h"
#include "tracker/udp_router.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_hash(LOG_TRACKER_REQUESTS, info().info_hash, "tracker_udp", "%p : " log_fmt, static_cast<TrackerWorker*>(this), __VA_ARGS__);

#define LT_LOG_DUMP(log_dump_data, log_dump_size, log_fmt, ...)         \
  lt_log_print_hash_dump(LOG_TRACKER_DUMP, log_dump_data, log_dump_size, info().info_hash, \
                         "tracker_udp", "%p : " log_fmt, static_cast<TrackerWorker*>(this), __VA_ARGS__);

namespace torrent {

// TODO: Handle network config changes.

TrackerUdp::TrackerUdp(const TrackerInfo& raw_info, int flags) :
  TrackerWorker(raw_info, flags) {

  if (info().key == 0)
    throw internal_error("TrackerUdp cannot be created with key 0.");

  auto [hostname, port] = net::parse_uri_host_port(info().url);

  m_hostname = std::move(hostname);
  m_port     = port;
}

TrackerUdp::~TrackerUdp() {
  close_directly();
}

bool
TrackerUdp::is_busy() const {
  return m_inet_transaction_id != 0 || m_inet6_transaction_id != 0;
}

void
TrackerUdp::send_event(tracker::TrackerState::event_enum new_state) {
  LT_LOG("sending event : state:%s url:%s", option_as_string(OPTION_TRACKER_EVENT, new_state), info().url.c_str());

  close_directly();

  lock_and_set_latest_event(new_state);

  if (m_hostname.empty())
    return handle_setup_error("cannot send tracker event, hostname is empty");

  if (m_port == 0)
    return handle_setup_error("cannot send tracker event, port is 0");

  m_send_state = new_state;

  connect_family(AF_INET);
  connect_family(AF_INET6);

  LT_LOG("started announce : state:%s url:%s inet_tx:%u inet6_tx:%u",
         option_as_string(OPTION_TRACKER_EVENT, new_state), info().url.c_str(),
         m_inet_transaction_id, m_inet6_transaction_id);

  if (m_inet_transaction_id == 0 && m_inet6_transaction_id == 0)
    return handle_setup_error("cannot send tracker event, no available network protocol(s)");
}

void
TrackerUdp::send_scrape() {
  throw internal_error("Tracker type UDP does not support scrape.");
}

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

  if (m_inet_transaction_id != 0) {
    ThreadTracker::thread_tracker()->udp_inet_router()->disconnect(m_inet_transaction_id);

    m_inet_transaction_id = 0;
    m_inet_connection_id  = 0;
  }

  if (m_inet6_transaction_id != 0) {
    ThreadTracker::thread_tracker()->udp_inet_router()->disconnect(m_inet6_transaction_id);

    m_inet6_transaction_id = 0;
    m_inet6_connection_id  = 0;
  }

  m_slot_close();
}

void
TrackerUdp::reset_family_with_error(int family, const std::string& msg) {
  switch (family) {
  case AF_INET:
    if (m_inet_transaction_id == 0)
      return; // TODO: Should we throw?

    m_inet_transaction_id = 0;
    m_inet_connection_id  = 0;
    break;
  case AF_INET6:
    if (m_inet6_transaction_id == 0)
      return; // TODO: Should we throw?

    m_inet6_transaction_id = 0;
    m_inet6_connection_id  = 0;
    break;
  default:
    throw internal_error("TrackerUdp::reset_family_with_error() called with invalid address family.");
  }

  if (m_inet_transaction_id != 0 || m_inet6_transaction_id != 0)
    return; // TODO: Save message.

  LT_LOG("closing with error : hostname:%s port:%u : %s", m_hostname.c_str(), m_port, msg.c_str());

  m_slot_close();
  m_slot_failure(msg);
}

tracker_enum
TrackerUdp::type() const {
  return TRACKER_UDP;
}

uint64_t&
TrackerUdp::connection_id_for_family(int family) {
  switch (family) {
  case AF_INET:
    return m_inet_connection_id;
  case AF_INET6:
    return m_inet6_connection_id;
  default:
    throw internal_error("TrackerUdp::connection_id_for_family() called with invalid address family.");
  }
}

uint32_t&
TrackerUdp::transaction_id_for_family(int family) {
  switch (family) {
  case AF_INET:
    return m_inet_transaction_id;
  case AF_INET6:
    return m_inet6_transaction_id;
  default:
    throw internal_error("TrackerUdp::transaction_id_for_family() called with invalid address family.");
  }
}

tracker::UdpRouter*
TrackerUdp::router_for_family(int family) {
  switch (family) {
  case AF_INET:
    return ThreadTracker::thread_tracker()->udp_inet_router();
  case AF_INET6:
    return ThreadTracker::thread_tracker()->udp_inet6_router();
  default:
    throw internal_error("TrackerUdp::router_for_family() called with invalid address family.");
  }
}

void
TrackerUdp::connect_family(int family) {
  auto new_id = router_for_family(family)->connect(
    m_hostname, m_port,
    [family, this](uint32_t id, auto& buffer)               { return prepare_connect(family, id, buffer); },
    [family, this](uint32_t id, auto& buffer)               { return process_connect(family, id, buffer); },
    [family, this](uint32_t id, int errno_err, int gai_err) { return handle_udp_error(family, id, errno_err, gai_err); });

  transaction_id_for_family(family) = new_id;
}

int
TrackerUdp::process_header(int family, uint32_t action, buffer_type& buffer) {
  if (buffer.size_end() < 8)
    return 0;

  uint32_t read_action    = buffer.read_32();
  uint32_t transaction_id = buffer.read_32();

  if (transaction_id != transaction_id_for_family(family))
    return 0;

  if (read_action == 3) {
    process_error(family, transaction_id, buffer);
    return -1;
  }

  if (read_action != action)
    return 0;

  return 1;
}

void
TrackerUdp::prepare_connect(int family, uint32_t id, buffer_type& buffer) {
  if (id != transaction_id_for_family(family))
    throw internal_error("TrackerUdp::prepare_connect() called with wrong transaction id.");

  buffer.write_64(magic_connection_id);
  buffer.write_32(0);
  buffer.write_32(id);

  LT_LOG_DUMP(buffer.begin(), buffer.size_end(), "prepare connect : family:%s id:%" PRIx32, family_str(family), id);
}

bool
TrackerUdp::process_connect(int family, uint32_t id, buffer_type& buffer) {
  LT_LOG_DUMP(buffer.begin(), buffer.size_end(), "process connect : family:%s id:%" PRIx32, family_str(family), id);

  switch (process_header(family, 0, buffer)) {
  case -1:
    return false;
  case 0:
    return true;
  };

  if (buffer.size_end() < 16)
    return handle_parse_error(family, id, "invalid connect response size");

  connection_id_for_family(family) = buffer.read_64();

  if (connection_id_for_family(family) == 0)
    return handle_parse_error(family, id, "connection id is 0");

  router_for_family(family)->transfer(id,
    [family, this](uint32_t id, auto& buffer)               { return prepare_announce(family, id, buffer); },
    [family, this](uint32_t id, auto& buffer)               { return process_announce(family, id, buffer); },
    [family, this](uint32_t id, int errno_err, int gai_err) { return handle_udp_error(family, id, errno_err, gai_err); },
    [family, this](uint32_t id)                             { transaction_id_for_family(family) = id; });

  return true;
}

void
TrackerUdp::prepare_announce(int family, uint32_t id, buffer_type& buffer) {
  if (id != transaction_id_for_family(family))
    throw internal_error("TrackerUdp::prepare_announce() called with wrong transaction id.");

  buffer.write_64(connection_id_for_family(family));
  buffer.write_32(1);
  buffer.write_32(transaction_id_for_family(family));

  buffer.write_range(info().info_hash.begin(), info().info_hash.end());
  buffer.write_range(info().local_id.begin(), info().local_id.end());

  auto parameters = m_slot_parameters();

  buffer.write_64(parameters.completed_adjusted);
  buffer.write_64(parameters.download_left);
  buffer.write_64(parameters.uploaded_adjusted);
  buffer.write_32(m_send_state);

  buffer.write_32_n([family]() -> uint32_t {
      if (family != AF_INET)
        return 0;

      auto local_address = config::network_config()->local_inet_address();

      if (local_address == nullptr || local_address->sa_family != AF_INET)
        return 0;

      return reinterpret_cast<const sockaddr_in*>(local_address.get())->sin_addr.s_addr;
    }());

  buffer.write_32(info().key);
  buffer.write_32(parameters.numwant);
  buffer.write_16(runtime::listen_port());

  if (buffer.size_end() != 98)
    throw internal_error("TrackerUdp::prepare_announce() unexpected buffer size.");

  LT_LOG_DUMP(buffer.begin(), buffer.size_end(),
              "prepare announce : state:%s family:%s id:%" PRIx32 " up_adj:%" PRIu64 " completed_adj:%" PRIu64 " left_adj:%" PRIu64,
              option_as_string(OPTION_TRACKER_EVENT, m_send_state), family_str(family), transaction_id_for_family(family),
              parameters.uploaded_adjusted, parameters.completed_adjusted, parameters.download_left);
}

bool
TrackerUdp::process_announce(int family, uint32_t id, buffer_type& buffer) {
  LT_LOG_DUMP(buffer.begin(), buffer.size_end(), "process announce : state:%s family:%s id:%" PRIx32,
              option_as_string(OPTION_TRACKER_EVENT, m_send_state), family_str(family), id);

  switch (process_header(family, 1, buffer)) {
  case -1:
    return false;
  case 0:
    return true;
  };

  if (buffer.size_end() < 20)
    return handle_parse_error(family, id, "invalid announce response size");

  {
    auto guard = lock_guard();

    state().set_normal_interval(buffer.read_32());
    state().set_min_interval(tracker::TrackerState::default_min_interval);

    state().m_scrape_incomplete = buffer.read_32(); // leechers
    state().m_scrape_complete   = buffer.read_32(); // seeders
    state().m_scrape_time_last  = this_thread::cached_seconds().count();
  }

  AddressList l;

  // TODO: This might not handle IPv4-mapped IPv6 addresses correctly.

  switch (family) {
  case AF_INET:
    std::copy(reinterpret_cast<const SocketAddressCompact*>(buffer.position()),
              reinterpret_cast<const SocketAddressCompact*>(buffer.end() - buffer.remaining() % sizeof(SocketAddressCompact)),
              std::back_inserter(l));

    m_inet_transaction_id = 0;
    m_inet_connection_id  = 0;
    break;

  case AF_INET6:
    std::copy(reinterpret_cast<const SocketAddressCompact6*>(buffer.position()),
              reinterpret_cast<const SocketAddressCompact6*>(buffer.end() - buffer.remaining() % sizeof(SocketAddressCompact6)),
              std::back_inserter(l));

    m_inet6_transaction_id = 0;
    m_inet6_connection_id  = 0;
    break;

  default:
    throw internal_error("TrackerUdp::process_announce_output() m_current_address is not inet or inet6.");
  }

  if (m_inet_transaction_id != 0 || m_inet6_transaction_id != 0) {
    LT_LOG("received announce response : family:%s hostname:%s port:%u peers:%zu", family_str(family), m_hostname.c_str(), m_port, l.size());

    m_slot_new_peers(std::move(l));
    return false;
  }

  LT_LOG("received announce success : family:%s hostname:%s port:%u peers:%zu", family_str(family), m_hostname.c_str(), m_port, l.size());

  m_slot_close();
  m_slot_success(std::move(l));

  return false;
}

void
TrackerUdp::process_error(int family, [[maybe_unused]] uint32_t id, buffer_type& buffer) {
  std::string msg(buffer.position(), buffer.end());

  if (msg.empty())
    msg = "empty error message";

  reset_family_with_error(family, "tracker message: " + msg);
}

void
TrackerUdp::handle_setup_error(const std::string& msg) {
  LT_LOG("setup error : hostname:%s port:%u : %s", m_hostname.c_str(), m_port, msg.c_str());

  if (m_inet_transaction_id != 0 || m_inet6_transaction_id != 0)
    throw internal_error("TrackerUdp::handle_setup_error() called but inet/inet6 transaction id is not 0.");

  m_slot_close();
  m_slot_failure(msg);
}

bool
TrackerUdp::handle_parse_error(int family, uint32_t id, const std::string& msg) {
  reset_family_with_error(family, "parse error: " + msg);
  return false;
}

void
TrackerUdp::handle_udp_error(int family, uint32_t id, int errno_err, int gai_err) {
  std::string msg = "network error: ";

  // TODO: Add flag to reset_family_with_error() to indicate the error is low-siginificance if it's
  // gai, so we show the other.

  if (errno_err != 0)
    msg += system::errno_enum(errno_err);
  else if (gai_err != 0)
    msg += net::gai_enum_error(gai_err);
  else
    msg += "unknown error";

  reset_family_with_error(family, msg);
}


} // namespace torrent
