#include "config.h"

#include "tracker/tracker_udp.h"

#include <cstdio>
#include <netdb.h>

#include "manager.h"
#include "net/address_list.h"
#include "torrent/net/network_config.h"
#include "torrent/net/resolver.h"
#include "torrent/net/socket_address.h"
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

// TODO: Rewrite this to do resolve every time, since we now have a cache?
// TODO: Add UDP listening socket used by all UDP trackers, make it handle retries and timeouts. It waits for reply.

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
    return receive_failed("cannot send tracker event, hostname is empty");

  if (m_port == 0)
    return receive_failed("cannot send tracker event, port is 0");

  m_send_state = new_state;

  m_inet_transaction_id = ThreadTracker::thread_tracker()->udp_inet_router()->connect(
    m_hostname, m_port,
    [this](uint32_t transaction_id, auto& buffer)               { return prepare_connect(transaction_id, buffer); },
    [this](uint32_t transaction_id, auto& buffer)               { return process_connect(transaction_id, buffer); },
    [this](uint32_t transaction_id, int errno_err, int gai_err) { return process_error(transaction_id, errno_err, gai_err); });

  LT_LOG("started announce : state:%s url:%s inet_tx:%u inet6_tx:%u",
         option_as_string(OPTION_TRACKER_EVENT, new_state), info().url.c_str(),
         m_inet_transaction_id, m_inet6_transaction_id);

  if (m_inet_transaction_id == 0 && m_inet6_transaction_id == 0)
    return receive_failed("cannot send tracker event, no available network protocol(s)");
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

  m_slot_close();

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
}

tracker_enum
TrackerUdp::type() const {
  return TRACKER_UDP;
}

void
TrackerUdp::receive_failed(const std::string& msg) {
  LT_LOG("received failure : hostname:%s port:%u : %s", m_hostname.c_str(), m_port, msg.c_str());

  close_directly();
  m_slot_failure(msg);
}

// void
// TrackerUdp::event_read() {
//   auto read_size = read_datagram(m_read_buffer->begin(), m_read_buffer->reserved());

//   if (read_size < 0)
//     return;

//   m_read_buffer->reset_position();
//   m_read_buffer->set_end(read_size);

//   LT_LOG("received reply : size:%d", read_size);
//   LT_LOG_DUMP(reinterpret_cast<const char*>(m_read_buffer->begin()), read_size, "received reply", 0);

//   if (read_size < 4)
//     return;

//   // Do something with the content here.
//   switch (m_read_buffer->read_32()) {
//   case 0:
//     if (m_action != 0 || !process_connect_output())
//       return;

//     prepare_announce_input();

//     this_thread::scheduler()->update_wait_for_ceil_seconds(&m_task_timeout, std::chrono::seconds(udp_timeout));

//     m_tries = udp_tries;
//     this_thread::event_insert_write(this);
//     return;

//   case 1:
//     if (m_action != 1 || !process_announce_output())
//       return;

//     return;

//   case 3:
//     if (!process_error_output())
//       return;

//     return;

//   default:
//     return;
//   }
// }

void
TrackerUdp::prepare_connect(uint32_t id, buffer_type& buffer) {
  if (id != m_inet_transaction_id)
    throw internal_error("TrackerUdp::prepare_connect() called with wrong transaction id.");

  buffer.write_64(magic_connection_id);
  buffer.write_32(0);
  buffer.write_32(id);

  LT_LOG_DUMP(buffer.begin(), buffer.size_end(), "prepare connect (id:%" PRIx32 ")", id);
}

bool
TrackerUdp::process_connect(uint32_t id, buffer_type& buffer) {
  if (id != m_inet_transaction_id)
    throw internal_error("TrackerUdp::process_connect() called with wrong transaction id.");

  if (buffer.size_end() < 16)
    return false;

  uint32_t action         = buffer.read_32();
  uint32_t transaction_id = buffer.read_32();

  if (action != 0 || transaction_id != m_inet_transaction_id)
    return false;

  m_inet_connection_id = buffer.read_64();

  if (m_inet_connection_id == 0)
    return false;

  LT_LOG_DUMP(buffer.begin(), buffer.size_end(), "process connect", 0);

  return true;
}

// void
// TrackerUdp::prepare_announce_input() {
//   m_write_buffer->reset();

//   m_write_buffer->write_64(m_connection_id);
//   m_write_buffer->write_32(m_action = 1);
//   m_write_buffer->write_32(m_transaction_id = random());

//   m_write_buffer->write_range(info().info_hash.begin(), info().info_hash.end());
//   m_write_buffer->write_range(info().local_id.begin(), info().local_id.end());

//   auto parameters = m_slot_parameters();

//   m_write_buffer->write_64(parameters.completed_adjusted);
//   m_write_buffer->write_64(parameters.download_left);
//   m_write_buffer->write_64(parameters.uploaded_adjusted);
//   m_write_buffer->write_32(m_send_state);

//   auto local_address = config::network_config()->local_inet_address();

//   // TODO: Set to 0 when sending using IPv6.
//   if (local_address->sa_family == AF_INET)
//     m_write_buffer->write_32_n(reinterpret_cast<const sockaddr_in*>(local_address.get())->sin_addr.s_addr);
//   else
//     m_write_buffer->write_32_n(0);

//   m_write_buffer->write_32(info().key);
//   m_write_buffer->write_32(parameters.numwant);
//   m_write_buffer->write_16(runtime::listen_port());

//   if (m_write_buffer->size_end() != 98)
//     throw internal_error("TrackerUdp::prepare_announce_input() ended up with the wrong size");

//   LT_LOG_DUMP(m_write_buffer->begin(), m_write_buffer->size_end(),
//               "prepare announce (state:%s id:%" PRIx32 " up_adj:%" PRIu64 " completed_adj:%" PRIu64 " left_adj:%" PRIu64 ")",
//               option_as_string(OPTION_TRACKER_EVENT, m_send_state),
//               m_transaction_id, parameters.uploaded_adjusted, parameters.completed_adjusted, parameters.download_left);
// }

// bool
// TrackerUdp::process_announce_output() {
//   if (m_read_buffer->size_end() < 20 ||
//       m_read_buffer->read_32() != m_transaction_id)
//     return false;

//   {
//     auto guard = lock_guard();

//     state().set_normal_interval(m_read_buffer->read_32());
//     state().set_min_interval(tracker::TrackerState::default_min_interval);

//     state().m_scrape_incomplete = m_read_buffer->read_32(); // leechers
//     state().m_scrape_complete   = m_read_buffer->read_32(); // seeders
//     state().m_scrape_time_last  = this_thread::cached_seconds().count();
//   }

//   AddressList l;

//   // TODO: This might not handle IPv4-mapped IPv6 addresses correctly.

//   switch (m_current_address->sa_family) {
//   case AF_INET:
//     std::copy(reinterpret_cast<const SocketAddressCompact*>(m_read_buffer->position()),
//               reinterpret_cast<const SocketAddressCompact*>(m_read_buffer->end() - m_read_buffer->remaining() % sizeof(SocketAddressCompact)),
//               std::back_inserter(l));
//     break;
//   case AF_INET6:
//     std::copy(reinterpret_cast<const SocketAddressCompact6*>(m_read_buffer->position()),
//               reinterpret_cast<const SocketAddressCompact6*>(m_read_buffer->end() - m_read_buffer->remaining() % sizeof(SocketAddressCompact6)),
//               std::back_inserter(l));
//     break;
//   default:
//     throw internal_error("TrackerUdp::process_announce_output() m_current_address is not inet or inet6.");
//   }

//   // Some logic here to decided on whetever we're going to close the
//   // connection or not?
//   close_directly();

//   m_slot_success(std::move(l));

//   return true;
// }

// bool
// TrackerUdp::process_error_output() {
//   if (m_read_buffer->size_end() < 8 ||
//       m_read_buffer->read_32() != m_transaction_id)
//     return false;

//   receive_failed("received error message: " + std::string(m_read_buffer->position(), m_read_buffer->end()));
//   return true;
// }

void
TrackerUdp::process_error(uint32_t id, int errno_err, int gai_err) {
  if (id != m_inet_transaction_id)
    throw internal_error("TrackerUdp::process_error() called with wrong transaction id.");

  std::string msg = "udp router error: ";

  if (errno_err != 0)
    msg += system::errno_enum(errno_err);
  else if (gai_err != 0)
    msg += net::gai_enum_error(gai_err);
  else
    msg += "unknown error";

  receive_failed(msg);
}


} // namespace torrent
