#include "config.h"

#include "listen.h"

#include "manager.h"
#include "protocol/handshake.h"
#include "torrent/exceptions.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/runtime/socket_manager.h"
#include "torrent/system/poll.h"
#include "torrent/system/types.h"
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
    throw resource_error("Could not open stream socket for listening: " + system::errno_enum_str(errno));

  int datagram_fd = fd_open(fd_flag_datagram | open_flags);

  if (datagram_fd == -1) {
    fd_close(stream_fd);
    throw resource_error("Could not open datagram socket for listening: " + system::errno_enum_str(errno));
  }

  return std::make_tuple(stream_fd, datagram_fd);
}

std::tuple<int, uint16_t>
listen_open_range(uint16_t first, uint16_t last, bool check_dht, const sockaddr* bind_address) {
  sa_unique_ptr try_address = sa_copy(bind_address);

  auto [stream_fd, datagram_fd] = listen_fd_open(bind_address);

  do {
    sa_set_port(try_address.get(), first);

    if (!fd_bind(stream_fd, try_address.get()))
      continue;

    // Try to bind the datagram socket to the same port to verify DHT availability.
    if (check_dht && !fd_bind(datagram_fd, try_address.get())) {
      LT_LOG("failed to bind datagram socket on port %" PRIu16 " to verify DHT availability : %s", first, system::errno_enum(errno));

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
Listen::open_single(Listen* listen, const sockaddr* bind_address, open_options options) {
  listen->close();

  if (options.first_port == 0 || options.first_port > options.last_port)
    throw input_error("Tried to open listening port with an invalid range");

  if (bind_address->sa_family != AF_INET && bind_address->sa_family != AF_INET6)
    throw input_error("Listening socket must be inet or inet6 address type");

  auto [listen_fd, listen_port] = listen_open_range(options.first_port, options.last_port, options.check_dht, bind_address);

  if (listen_fd == -1) {
    LT_LOG("failed to find a suitable listen port : options.last_port error : %s", system::errno_enum(errno));
    return false;
  }

  auto error_fn = [listen_fd](std::string msg) {
      fd_close(listen_fd);

      LT_LOG("%s : %s", msg.c_str(), system::errno_enum(errno));
      throw resource_error(msg + ": " + std::string(strerror(errno)));
    };

  if (options.block_ipv4in6 && (bind_address->sa_family == AF_INET6) && !fd_set_v6only(listen_fd, true))
    error_fn("Could not set IPV6_V6ONLY on socket");

  if (!fd_listen(listen_fd, options.backlog))
    error_fn("Could not listen on socket");

  listen->open_done(listen_fd, listen_port, options.backlog);
  return true;
}

bool
Listen::open_both(Listen* listen_inet, Listen* listen_inet6, const sockaddr* bind_inet_address, const sockaddr* bind_inet6_address, open_options options) {
  listen_inet->close();
  listen_inet6->close();

  if (options.first_port == 0 || options.first_port > options.last_port)
    throw input_error("Tried to open listening port with an invalid range");

  if (bind_inet_address->sa_family != AF_INET)
    throw input_error("Listening ipv4 socket must be inet address type");

  if (bind_inet6_address->sa_family != AF_INET6)
    throw input_error("Listening ipv6 socket must be inet6 address type");

  int      inet_fd{},   inet6_fd{};
  uint16_t inet_port{}, inet6_port{};

  while (true) {
    if (options.first_port > options.last_port) {
      LT_LOG("Unable to find suitable dual ipv4+6 listen ports in the given range", 0);
      return false;
    }

    std::tie(inet_fd, inet_port) = listen_open_range(options.first_port, options.last_port, options.check_dht, bind_inet_address);

    if (inet_fd == -1) {
      LT_LOG("Unable to find a suitable ipv4 listen port : options.last_port error : %s", system::errno_enum(errno));
      return false;
    }

    std::tie(inet6_fd, inet6_port) = listen_open_range(inet_port, inet_port, options.check_dht, bind_inet6_address);

    if (inet6_fd != -1)
      break;

    fd_close(inet_fd);
    options.first_port = inet_port + 1;

    LT_LOG("Unable to find a suitable ipv6 listen port matching ipv4 port %" PRIu16 " : options.last_port error : %s", inet_port, system::errno_enum(errno));
  }

  try {
    auto error_fn = [](std::string msg) {
        LT_LOG("%s : %s", msg.c_str(), system::errno_enum(errno));
        throw resource_error(msg + ": " + std::string(strerror(errno)));
      };


    if (inet_port != inet6_port)
      throw internal_error("Listen::open_both(): ipv4 and ipv6 ports do not match.");

    if (!fd_listen(inet_fd, options.backlog))
      error_fn("Could not listen on ipv4 socket");

    if (options.fallback_to_single) {
      if (!fd_set_v6only(inet6_fd, true)) {
        if (errno != EADDRINUSE)
          error_fn("Could not set IPV6_V6ONLY on ipv6 socket");

        LT_LOG("Could not set IPV6_V6ONLY on ipv6 socket, falling back to single unspec ipv6 listen", 0);

        fd_close(inet_fd);
        inet_fd = -1;

        if (!fd_set_v6only(inet6_fd, true))
          error_fn("Could not set IPV6_V6ONLY on ipv6 socket for fallback");
      }

    } else if (options.block_ipv4in6) {
      if (!fd_set_v6only(inet6_fd, true))
        error_fn("Could not set IPV6_V6ONLY on ipv6 socket");
    }

    if (!fd_listen(inet6_fd, options.backlog))
      error_fn("Could not listen on ipv6 socket");

  } catch (...) {
    if (inet_fd != -1)
      fd_close(inet_fd);

    fd_close(inet6_fd);
    throw;
  }

  if (inet_fd != -1)
    listen_inet->open_done(inet_fd, inet_port, options.backlog);

  listen_inet6->open_done(inet6_fd, inet_port, options.backlog);
  return true;
}

void
Listen::open_done(int fd, uint16_t port, int backlog) {
  set_file_descriptor(fd);
  m_port = port;

  runtime::socket_manager()->register_event_or_throw(this, runtime::category_internal, [this]() {
      this_thread::poll()->open(this);
      this_thread::poll()->insert_read(this);
    });

  LT_LOG("listen opened: fd:%i port:%" PRIu16 " backlog:%i", file_descriptor(), m_port, backlog);
}

void Listen::close() {
  if (!is_open())
    return;

  runtime::socket_manager()->unregister_event_or_throw(this, [this]() {
      this_thread::poll()->remove_and_close(this);

      fd_close(file_descriptor());
      reset_file_descriptor();
    });

  m_port = 0;
}

void
Listen::event_read() {
  while (true) {
    auto handshake = std::make_unique<Handshake>();

    // TODO: Optimize this by adding handshake immediately to poll and put the slot_accepted() call
    // outside of open_func().

    int           fd;
    sa_unique_ptr sa;

    std::tie(fd, sa) = fd_sap_accept(file_descriptor());

    if (fd == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        return;

      // Force a new event_read() call just to be sure we don't enter an infinite loop.
      if (errno == ECONNABORTED)
        return;

      throw resource_error("Listener port accept() failed: " + system::errno_enum_str(errno));
    }

    auto open_func = [&]() {
        // TODO: Figure out a clean way of doing this.
        int tmp_fd = fd;
        fd = -1;

        m_slot_accepted(handshake, tmp_fd, sa.get());
      };

    auto cleanup_func = [fd, &handshake](bool opened) {
        LT_LOG("failed to accept incoming connection : socket manager triggered cleanup", 0);

        if (!opened) {
          fd_close(fd);
          return;
        }

        if (handshake && handshake->is_open())
          handshake->destroy_connection(false);
      };

    // TODO: This needs to be handled differently as open_ isn't done, we're doing accept_event_or_cleanup? Add a close callback?

    bool result = runtime::socket_manager()->open_event_or_cleanup(handshake.get(), runtime::category_generic, open_func, cleanup_func);

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
    throw internal_error("Listener port could not get socket error: " + system::errno_enum_str(errno));

  if (socket_error != 0)
    throw internal_error("Listener port received an error event: " + system::errno_enum_str(socket_error));
}

} // namespace torrent
