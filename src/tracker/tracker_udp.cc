#include "config.h"

#define __STDC_FORMAT_MACROS

#include "tracker_udp.h"

#include <cstdio>
#include <sys/types.h>

#include "manager.h"
#include "net/address_list.h"
#include "rak/error_number.h"
#include "torrent/exceptions.h"
#include "torrent/connection_manager.h"
#include "torrent/download_info.h"
#include "torrent/poll.h"
#include "torrent/tracker_list.h"
#include "torrent/net/resolver.h"
#include "torrent/utils/log.h"
#include "torrent/utils/option_strings.h"
#include "torrent/utils/thread.h"
#include "torrent/utils/uri_parser.h"

#define LT_LOG_TRACKER_REQUESTS(log_fmt, ...)                             \
  lt_log_print_hash(LOG_TRACKER_REQUESTS, info().info_hash, "tracker_udp", log_fmt, __VA_ARGS__);

#define LT_LOG_TRACKER_DUMP(log_level, log_dump_data, log_dump_size, log_fmt, ...)                   \
  lt_log_print_hash_dump(LOG_TRACKER_DUMP, log_dump_data, log_dump_size, info().info_hash, "tracker_udp", log_fmt, __VA_ARGS__);

namespace torrent {

TrackerUdp::TrackerUdp(const TrackerInfo& info, int flags) :
  TrackerWorker(info, flags) {

  m_taskTimeout.slot() = std::bind(&TrackerUdp::receive_timeout, this);
}

TrackerUdp::~TrackerUdp() {
  close_directly();
}

bool
TrackerUdp::is_busy() const {
  return get_fd().is_valid();
}

void
TrackerUdp::send_event(tracker::TrackerState::event_enum new_state) {
  close_directly();

  hostname_type hostname;

  if (!parse_udp_url(info().url, hostname, m_port))
    return receive_failed("could not parse hostname or port");

  lock_and_set_latest_event(new_state);
  m_send_state = new_state;
  m_resolver_requesting = true;

  LT_LOG_TRACKER_REQUESTS("hostname lookup (address:%s)", hostname.data());

  // Currently discarding SOCK_DGRAM.
  thread_self->resolver()->resolve(this, hostname.data(), PF_UNSPEC,
                                   [this](const sockaddr* sa, int err) {
                                     start_announce(sa, err);
                                   });
}

void
TrackerUdp::send_scrape() {
  throw internal_error("Tracker type UDP does not support scrape.");
}

bool
TrackerUdp::parse_udp_url(const std::string& url, hostname_type& hostname, int& port) const {
  if (std::sscanf(url.c_str(), "udp://%1023[^:]:%i", hostname.data(), &port) == 2 && hostname[0] != '\0' &&
      port > 0 && port < (1 << 16))
    return true;

  if (std::sscanf(url.c_str(), "udp://[%1023[^]]]:%i", hostname.data(), &port) == 2 && hostname[0] != '\0' &&
      port > 0 && port < (1 << 16))
    return true;

  return false;
}

void
TrackerUdp::start_announce(const sockaddr* sa, [[maybe_unused]] int err) {
  m_resolver_requesting = false;

  if (sa == NULL)
    return receive_failed("could not resolve hostname");

  m_connectAddress = *rak::socket_address::cast_from(sa);
  m_connectAddress.set_port(m_port);

  LT_LOG_TRACKER_REQUESTS("address found (address:%s)", m_connectAddress.address_str().c_str());

  if (!m_connectAddress.is_valid())
    return receive_failed("invalid tracker address");

  // TODO: Make each of these a separate error... at the very least separate open and bind.
  if (!get_fd().open_datagram() || !get_fd().set_nonblock())
    return receive_failed("could not open UDP socket");

  auto bind_address = rak::socket_address::cast_from(manager->connection_manager()->bind_address());

  if (bind_address->is_bindable() && !get_fd().bind(*bind_address))
    return receive_failed("failed to bind socket to udp address '" + bind_address->pretty_address_str() + "' with error '" + rak::error_number::current().c_str() + "'");

  m_readBuffer = new ReadBuffer;
  m_writeBuffer = new WriteBuffer;

  prepare_connect_input();

  thread_self->poll()->open(this);
  thread_self->poll()->insert_read(this);
  thread_self->poll()->insert_write(this);
  thread_self->poll()->insert_error(this);

  m_tries = udp_tries;
  priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(udp_timeout)).round_seconds());
}

void
TrackerUdp::close() {
  if (!get_fd().is_valid())
    return;

  LT_LOG_TRACKER_REQUESTS("request cancelled (state:%s url:%s)",
                          option_as_string(OPTION_TRACKER_EVENT, state().latest_event()), info().url.c_str());

  close_directly();
}

void
TrackerUdp::disown() {
  if (!get_fd().is_valid())
    return;

  LT_LOG_TRACKER_REQUESTS("request disowned (state:%s url:%s)",
                          option_as_string(OPTION_TRACKER_EVENT, state().latest_event()), info().url.c_str());

  close_directly();
}

void
TrackerUdp::close_directly() {
  if (!get_fd().is_valid())
    return;

  if (m_resolver_requesting) {
    thread_self->resolver()->cancel(this);
    m_resolver_requesting = false;
  }

  delete m_readBuffer;
  delete m_writeBuffer;

  m_readBuffer = NULL;
  m_writeBuffer = NULL;

  priority_queue_erase(&taskScheduler, &m_taskTimeout);

  thread_self->poll()->remove_read(this);
  thread_self->poll()->remove_write(this);
  thread_self->poll()->remove_error(this);
  thread_self->poll()->close(this);

  get_fd().close();
  get_fd().clear();
}

tracker_enum
TrackerUdp::type() const {
  return TRACKER_UDP;
}

void
TrackerUdp::receive_failed(const std::string& msg) {
  close_directly();
  m_slot_failure(msg);
}

void
TrackerUdp::receive_timeout() {
  if (m_taskTimeout.is_queued())
    throw internal_error("TrackerUdp::receive_timeout() called but m_taskTimeout is still scheduled.");

  if (--m_tries == 0) {
    receive_failed("unable to connect to UDP tracker");
  } else {
    priority_queue_insert(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(udp_timeout)).round_seconds());

    thread_self->poll()->insert_write(this);
  }
}

void
TrackerUdp::event_read() {
  rak::socket_address sa;

  int s = read_datagram(m_readBuffer->begin(), m_readBuffer->reserved(), &sa);

  if (s < 0)
    return;

  m_readBuffer->reset_position();
  m_readBuffer->set_end(s);

  LT_LOG_TRACKER_DUMP(DEBUG, (const char*)m_readBuffer->begin(), s, "received reply", 0);

  if (s < 4)
    return;

  // Make sure sa is from the source we expected?

  // Do something with the content here.
  switch (m_readBuffer->read_32()) {
  case 0:
    if (m_action != 0 || !process_connect_output())
      return;

    prepare_announce_input();

    priority_queue_update(&taskScheduler, &m_taskTimeout, (cachedTime + rak::timer::from_seconds(udp_timeout)).round_seconds());

    m_tries = udp_tries;
    thread_self->poll()->insert_write(this);
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
  };
}

void
TrackerUdp::event_write() {
  if (m_writeBuffer->size_end() == 0)
    throw internal_error("TrackerUdp::write() called but the write buffer is empty.");

  [[maybe_unused]] int s = write_datagram(m_writeBuffer->begin(), m_writeBuffer->size_end(), &m_connectAddress);

  thread_self->poll()->remove_write(this);
}

void
TrackerUdp::event_error() {
}

void
TrackerUdp::prepare_connect_input() {
  m_writeBuffer->reset();
  m_writeBuffer->write_64(m_connectionId = magic_connection_id);
  m_writeBuffer->write_32(m_action = 0);
  m_writeBuffer->write_32(m_transactionId = random());

  LT_LOG_TRACKER_DUMP(DEBUG, m_writeBuffer->begin(), m_writeBuffer->size_end(),
                      "prepare connect (id:%" PRIx32 ")", m_transactionId);
}

void
TrackerUdp::prepare_announce_input() {
  m_writeBuffer->reset();

  m_writeBuffer->write_64(m_connectionId);
  m_writeBuffer->write_32(m_action = 1);
  m_writeBuffer->write_32(m_transactionId = random());

  m_writeBuffer->write_range(info().info_hash.begin(), info().info_hash.end());
  m_writeBuffer->write_range(info().local_id.begin(), info().local_id.end());

  auto parameters = m_slot_parameters();

  m_writeBuffer->write_64(parameters.completed_adjusted);
  m_writeBuffer->write_64(parameters.download_left);
  m_writeBuffer->write_64(parameters.uploaded_adjusted);
  m_writeBuffer->write_32(m_send_state);

  const rak::socket_address* localAddress = rak::socket_address::cast_from(manager->connection_manager()->local_address());

  uint32_t local_addr = 0;

  if (localAddress->family() == rak::socket_address::af_inet)
    local_addr = localAddress->sa_inet()->address_n();

  m_writeBuffer->write_32_n(local_addr);
  m_writeBuffer->write_32(info().key);
  m_writeBuffer->write_32(parameters.numwant);
  m_writeBuffer->write_16(manager->connection_manager()->listen_port());

  if (m_writeBuffer->size_end() != 98)
    throw internal_error("TrackerUdp::prepare_announce_input() ended up with the wrong size");

  LT_LOG_TRACKER_DUMP(DEBUG, m_writeBuffer->begin(), m_writeBuffer->size_end(),
                      "prepare announce (state:%s id:%" PRIx32 " up_adj:%" PRIu64 " completed_adj:%" PRIu64 " left_adj:%" PRIu64 ")",
                      option_as_string(OPTION_TRACKER_EVENT, m_send_state),
                      m_transactionId, parameters.uploaded_adjusted, parameters.completed_adjusted, parameters.download_left);
}

bool
TrackerUdp::process_connect_output() {
  if (m_readBuffer->size_end() < 16 ||
      m_readBuffer->read_32() != m_transactionId)
    return false;

  m_connectionId = m_readBuffer->read_64();

  return true;
}

bool
TrackerUdp::process_announce_output() {
  if (m_readBuffer->size_end() < 20 ||
      m_readBuffer->read_32() != m_transactionId)
    return false;

  {
    auto guard = lock_guard();

    state().set_normal_interval(m_readBuffer->read_32());
    state().set_min_interval(tracker::TrackerState::default_min_interval);

    state().m_scrape_incomplete = m_readBuffer->read_32(); // leechers
    state().m_scrape_complete   = m_readBuffer->read_32(); // seeders
    state().m_scrape_time_last  = rak::timer::current().seconds();
  }

  AddressList l;

  std::copy(reinterpret_cast<const SocketAddressCompact*>(m_readBuffer->position()),
	    reinterpret_cast<const SocketAddressCompact*>(m_readBuffer->end() - m_readBuffer->remaining() % sizeof(SocketAddressCompact)),
	    std::back_inserter(l));

  // Some logic here to decided on whetever we're going to close the
  // connection or not?
  close_directly();

  m_slot_success(std::move(l));

  return true;
}

bool
TrackerUdp::process_error_output() {
  if (m_readBuffer->size_end() < 8 ||
      m_readBuffer->read_32() != m_transactionId)
    return false;

  receive_failed("received error message: " + std::string(m_readBuffer->position(), m_readBuffer->end()));
  return true;
}

}
