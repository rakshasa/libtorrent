#include "config.h"

#include "udp_router.h"

#include <iterator>

#include "torrent/net/fd.h"
#include "torrent/net/network_config.h"
#include "torrent/net/poll.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_subsystem(LOG_TRACKER_REQUESTS, "udp_router", log_fmt, __VA_ARGS__);
  // lt_log_print_subsystem(LOG_TRACKER_REQUESTS, "udp_router", "%p : " log_fmt, static_cast<TrackerWorker*>(this), __VA_ARGS__);

namespace torrent::tracker {

UdpRouter::UdpRouter() {
  std::random_device rd;
  std::mt19937       mt(rd());

  m_random_engine.seed(mt());
}

void
UdpRouter::open(int family) {
  if (is_open())
    return;

  if (family != AF_INET)
    throw internal_error("UdpRouter::open() called with unsupported family.");

  auto bind_address = config::network_config()->bind_address_for_connect(AF_INET);

  if (bind_address == nullptr) {
    LT_LOG("could not open udp router : blocked or invalid bind address : family:%s", family_str(family));
    return;
  }

  if (bind_address->sa_family != family)
    throw internal_error("UdpRouter::open() bind address family does not match requested family.");

  auto open_fd = [this, family, &bind_address]() {
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

      this_thread::event_open(this);
      this_thread::event_insert_read(this);
      // this_thread::event_insert_write(this);
      this_thread::event_insert_error(this);
    };

  auto cleanup_func = [this, family]() {
      if (!is_open())
        return;

      LT_LOG("opening router failed : socket manager triggered cleanup : family:%s", family_str(family));

      this_thread::event_remove_and_close(this);

      fd_close(file_descriptor());
      set_file_descriptor(-1);
    };

  if (!runtime::socket_manager()->open_event_or_cleanup(this, open_fd, cleanup_func))
    return;

  set_socket_address(sa_copy(bind_address.get()));

  LT_LOG("opened udp router : family:%s bind_address:%s", family_str(family), sa_pretty_str(bind_address.get()).c_str());
}

void
UdpRouter::close() {
  if (!is_open())
    return;

  runtime::socket_manager()->close_event_or_throw(this, [this]() {
      this_thread::event_remove_and_close(this);

      fd_close(file_descriptor());
      set_file_descriptor(-1);
    });

  LT_LOG("closed udp router", 0);
}

uint32_t
UdpRouter::connect(c_sa_shared_ptr address, prepare_func prepare_fn, process_func process_fn) {
  if (address->sa_family != socket_address()->sa_family)
    throw internal_error("UdpRouter::connect() called with unsupported address family.");

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

  itr->second.address   = std::move(address);
  itr->second.prepare   = std::move(prepare_fn);
  itr->second.process   = std::move(process_fn);
  itr->second.queue_itr = m_write_queue.end();

  if (!try_write(itr->first, itr->second))
    queue_write(itr->first, itr->second);

  return itr->first;
}

void
UdpRouter::disconnect(uint32_t id) {
  auto itr = m_connections.find(id);

  if (itr == m_connections.end())
    throw internal_error("UdpRouter::disconnect() called with invalid connection ID.");

  if (itr->second.queue_itr != m_write_queue.end())
    m_write_queue.erase(itr->second.queue_itr);

  m_connections.erase(itr);
}

bool
UdpRouter::try_write(uint32_t id, const connection_info& info) {
  if (!is_open())
    return false;

  m_buffer.reset();

  info.prepare(id, m_buffer);

  if (m_buffer.size_end() == 0)
    throw internal_error("UdpRouter::try_write() prepare function did not write any data.");

  auto bytes_written = write_datagram_sa(m_buffer.begin(), m_buffer.size_end(), info.address.get());

  if (bytes_written == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
      return false;

    LT_LOG("failed to write datagram : address:%s errno:%s", sa_pretty_str(info.address.get()).c_str(), system::errno_enum_str(errno).c_str());

    // TODO: Need to be handled differently.
    throw internal_error("UdpRouter::try_write() failed to write datagram: " + system::errno_enum_str(errno));
  }

  if (bytes_written != m_buffer.size_end()) {
    LT_LOG("failed to write datagram : address:%s : only %d of %d bytes written", sa_pretty_str(info.address.get()).c_str(), bytes_written, m_buffer.size_end());
    throw internal_error("UdpRouter::try_write() did not write entire datagram.");
  }

  return true;
}

void
UdpRouter::queue_write(uint32_t id, connection_info& info) {
  if (info.queue_itr != m_write_queue.end())
    throw internal_error("UdpRouter::queue_write() called for connection that is already queued for writing.");

  if (m_write_queue.empty())
    this_thread::event_insert_write(this);

  m_write_queue.emplace_back(id, info);

  info.queue_itr = std::prev(m_write_queue.end());
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
      LT_LOG("received datagram with invalid transaction ID : address:%s", sa_pretty_str(&from_sa.sa).c_str());
      continue;
    }

    auto itr = m_connections.find(transaction_id);

    if (itr == m_connections.end()) {
      LT_LOG("received datagram with unknown transaction ID : address:%s transaction_id:%" PRIx32, sa_pretty_str(&from_sa.sa).c_str(), transaction_id);
      continue;
    }

    itr->second.process(transaction_id, m_buffer);
  }
}

void
UdpRouter::event_write() {
  while (!m_write_queue.empty()) {
    auto& [id, info] = m_write_queue.front();

    if (!try_write(id, info))
      break;

    info.queue_itr = m_write_queue.end();
    m_write_queue.pop_front();
  }

  if (m_write_queue.empty())
    this_thread::event_remove_write(this);
}

void
UdpRouter::event_error() {
  LT_LOG("socket error event triggered", 0);
  throw internal_error("UdpRouter::event_error() triggered");
}

} // namespace torrent::tracker
