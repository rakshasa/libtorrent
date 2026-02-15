#include "config.h"

#include "listen.h"

#include "manager.h"
#include "protocol/handshake.h"
#include "torrent/connection_manager.h"
#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/net/poll.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/utils/log.h"

#define LT_LOG(log_fmt, ...)                                    \
  lt_log_print(torrent::LOG_CONNECTION_LISTEN, "listen: " log_fmt, __VA_ARGS__);

namespace torrent {

namespace {

std::tuple<int, int>
listen_fd_open(const sockaddr* bind_address) {
  fd_flags open_flags = fd_flag_nonblock | fd_flag_reuse_address;

  if (bind_address->sa_family == AF_INET)
    open_flags |= fd_flag_v4only;

  int stream_fd = fd_open(fd_flag_stream | open_flags);

  if (stream_fd == -1)
    throw resource_error("Could not open stream socket for listening: " + std::string(strerror(errno)));

  int datagram_fd = fd_open(fd_flag_datagram | open_flags);

  if (datagram_fd == -1) {
    fd_close(stream_fd);
    throw resource_error("Could not open datagram socket for listening: " + std::string(strerror(errno)));
  }

  return std::make_tuple(stream_fd, datagram_fd);
}

std::tuple<int, uint16_t>
listen_open_range(uint16_t first, uint16_t last, const sockaddr* bind_address) {
  sa_unique_ptr try_address = sa_copy(bind_address);

  auto [stream_fd, datagram_fd] = listen_fd_open(bind_address);

  do {
    sa_set_port(try_address.get(), first);

    if (!fd_bind(stream_fd, try_address.get()))
      continue;

    // Try to bind the datagram socket to the same port to verify DHT availability.
    if (!fd_bind(datagram_fd, try_address.get())) {
      LT_LOG("failed to bind datagram socket on port %" PRIu16 " to verify DHT availability : %s", first, std::strerror(errno));

      fd_close(stream_fd);
      fd_close(datagram_fd);

      std::tie(stream_fd, datagram_fd) = listen_fd_open(bind_address);
      continue;
    }

    fd_close(datagram_fd);
    return std::make_tuple(stream_fd, first);

  } while (first++ < last);

  fd_close(stream_fd);
  fd_close(datagram_fd);

  return std::make_tuple(-1, 0);
}

}

bool
Listen::open_single(Listen* listen, const sockaddr* bind_address, uint16_t first, uint16_t last, int backlog, bool block_ipv4in6) {
  listen->close();

  if (first == 0 || first > last)
    throw input_error("Tried to open listening port with an invalid range");

  if (bind_address->sa_family != AF_INET && bind_address->sa_family != AF_INET6)
    throw input_error("Listening socket must be inet or inet6 address type");

  auto [listen_fd, listen_port] = listen_open_range(first, last, bind_address);

  if (listen_fd == -1) {
    LT_LOG("failed to find a suitable listen port : last error : %s", std::strerror(errno));
    return false;
  }

  if (block_ipv4in6 && bind_address->sa_family == AF_INET6 && !fd_set_v6only(listen_fd, true)) {
    fd_close(listen_fd);
    LT_LOG("failed to set IPV6_V6ONLY on socket : %s", std::strerror(errno));
    throw resource_error("Could not set IPV6_V6ONLY on socket: " + std::string(strerror(errno)));
  }

  if (!fd_listen(listen_fd, backlog)) {
    fd_close(listen_fd);
    LT_LOG("failed to listen on socket : %s", std::strerror(errno));
    throw resource_error("Could not listen on socket: " + std::string(strerror(errno)));
  }

  listen->open_done(listen_fd, listen_port, backlog);
  return true;
}

bool
Listen::open_both(Listen* listen_inet, Listen* listen_inet6,
                  const sockaddr* bind_inet_address, const sockaddr* bind_inet6_address,
                  uint16_t first, uint16_t last, int backlog, bool block_ipv4in6) {
  listen_inet->close();
  listen_inet6->close();

  if (first == 0 || first > last)
    throw input_error("Tried to open listening port with an invalid range");

  if (bind_inet_address->sa_family != AF_INET)
    throw input_error("Listening ipv4 socket must be inet address type");

  if (bind_inet6_address->sa_family != AF_INET6)
    throw input_error("Listening ipv6 socket must be inet6 address type");

  int inet_fd{}, inet6_fd{};
  uint16_t inet_port{}, inet6_port{};

  while (true) {
    if (first > last) {
      LT_LOG("Unable to find suitable dual ipv4+6 listen ports in the given range", 0);
      return false;
    }

    std::tie(inet_fd, inet_port) = listen_open_range(first, last, bind_inet_address);

    if (inet_fd == -1) {
      LT_LOG("Unable to find a suitable ipv4 listen port : last error : %s", std::strerror(errno));
      return false;
    }

    std::tie(inet6_fd, inet6_port) = listen_open_range(inet_port, inet_port, bind_inet6_address);

    if (inet6_fd != -1)
      break;

    fd_close(inet_fd);
    first = inet_port + 1;

    LT_LOG("Unable to find a suitable ipv6 listen port matching ipv4 port %" PRIu16 " : last error : %s", inet_port, std::strerror(errno));
  }

  try {
    if (inet_port != inet6_port)
      throw internal_error("Listen::open_both(): ipv4 and ipv6 ports do not match.");

    if (!fd_listen(inet_fd, backlog)) {
      LT_LOG("failed to listen on ipv4 socket : %s", std::strerror(errno));
      throw resource_error("Could not listen on ipv4 socket: " + std::string(strerror(errno)));
    }

    if (block_ipv4in6 && !fd_set_v6only(inet6_fd, true)) {
      LT_LOG("failed to set IPV6_V6ONLY on socket : %s", std::strerror(errno));
      throw resource_error("Could not set IPV6_V6ONLY on socket: " + std::string(strerror(errno)));
    }

    if (!fd_listen(inet6_fd, backlog)) {
      LT_LOG("failed to listen on ipv6 socket : %s", std::strerror(errno));
      throw resource_error("Could not listen on ipv6 socket: " + std::string(strerror(errno)));
    }

  } catch (...) {
    fd_close(inet_fd);
    fd_close(inet6_fd);
    throw;
  }

  listen_inet->open_done(inet_fd, inet_port, backlog);
  listen_inet6->open_done(inet6_fd, inet_port, backlog);
  return true;
}

void
Listen::open_done(int fd, uint16_t port, int backlog) {
  set_file_descriptor(fd);
  m_port = port;

  manager->connection_manager()->inc_socket_count();

  runtime::socket_manager()->register_event_or_throw(this, [this]() {
      this_thread::poll()->open(this);
      this_thread::poll()->insert_read(this);
      this_thread::poll()->insert_error(this);
    });

  LT_LOG("listen opened: fd:%i port:%" PRIu16 " backlog:%i", file_descriptor(), m_port, backlog);
}

void Listen::close() {
  if (!is_open())
    return;

  runtime::socket_manager()->unregister_event_or_throw(this, [this]() {
      this_thread::poll()->remove_and_close(this);

      fd_close(file_descriptor());
      set_file_descriptor(-1);
    });

  manager->connection_manager()->dec_socket_count();

  m_port = 0;
}

void
Listen::event_read() {
  while (true) {
    auto handshake = std::make_unique<Handshake>();

    // TODO: Optimize this by adding handshake immediately to poll and put the slot_accepted() call
    // outside of open_func().

    auto open_func = [&]() {
        int fd;
        sa_unique_ptr sa;

        std::tie(fd, sa) = fd_sap_accept(file_descriptor());

        if (fd == -1) {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;

          // Force a new event_read() call just to be sure we don't enter an infinite loop.
          if (errno == ECONNABORTED)
            return;

          throw resource_error("Listener port accept() failed: " + std::string(std::strerror(errno)));
        }

        m_slot_accepted(handshake, fd, sa.get());
      };

    auto cleanup_func = [&]() {
        LT_LOG("failed to accept incoming connection : socket manager triggered cleanup", 0);

        if (handshake && handshake->is_open())
          handshake->destroy_connection(false);
      };

    bool result = runtime::socket_manager()->open_event_or_cleanup(handshake.get(), open_func, cleanup_func);

    // If the handshake connection or socket manager failed, don't continue accepting connections.
    //
    // This allows the event loop a chance to clear out any conflicts with reused file descriptors.

    if (!result)
      return;
  }
}

void
Listen::event_write() {
  throw internal_error("Listener does not support write().");
}

void
Listen::event_error() {
  int socket_error;

  if (!fd_get_socket_error(file_descriptor(), &socket_error))
    throw internal_error("Listener port could not get socket error: " + std::string(std::strerror(errno)));

  if (socket_error != 0)
    throw internal_error("Listener port received an error event: " + std::string(std::strerror(socket_error)));
}

} // namespace torrent
