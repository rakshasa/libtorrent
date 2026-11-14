#include "config.h"

#include "dht/udp_router.h"

#include <cstring>

#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/network_config.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/system/poll.h"
#include "torrent/utils/log.h"

#define LT_LOG_THIS(log_fmt, ...)                                       \
  lt_log_print(LOG_DHT_SERVER, "dht_server : " log_fmt, __VA_ARGS__);

namespace torrent::dht {

UdpRouter::UdpRouter(process_func process, write_func read_done, write_func write)
  : m_process(std::move(process)), m_read_done(std::move(read_done)), m_write(std::move(write)) {
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
}

void
UdpRouter::close() {
  if (!is_open())
    return;

  runtime::socket_manager()->close_event_or_throw(this, [this]() {
      this_thread::poll()->remove_and_close(this);
      fd_close_and_clear(file_descriptor_ref());
    });
}

void
UdpRouter::start_write() {
  this_thread::poll()->insert_write(this);
}

void
UdpRouter::stop_write() {
  this_thread::poll()->remove_write(this);
}

void
UdpRouter::event_read() {
  for (unsigned int count = 0; count < max_read_datagrams; ++count) {
    char buffer[2048];
    sockaddr_in6 sa_raw{};
    sockaddr* sa = reinterpret_cast<sockaddr*>(&sa_raw);
    int32_t read = read_datagram_sa(buffer, sizeof(buffer), sa, sizeof(sa_raw));

    if (read < 0)
      break;

    if (sa_is_v4mapped(sa)) {
      auto sa_unmapped = sin_from_v4mapped_in6(&sa_raw);
      *reinterpret_cast<sockaddr_in*>(&sa_raw) = *sa_unmapped.get();
    }

    if (sa->sa_family == socket_address()->sa_family)
      m_process(buffer, read, sa);
  }

  m_read_done();
}

void
UdpRouter::event_write() {
  m_write();
}

void
UdpRouter::event_error() {
}

} // namespace torrent::dht
