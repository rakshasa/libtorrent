#include "config.h"

#include "listen.h"

#include "manager.h"
#include "torrent/connection_manager.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

namespace torrent {

static std::tuple<int, int>
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

bool
Listen::open(uint16_t first, uint16_t last, int backlog, const sockaddr* bind_address) {
  close();

  if (first == 0 || first > last)
    throw input_error("Tried to open listening port with an invalid range");

  if (bind_address->sa_family != AF_INET && bind_address->sa_family != AF_INET6)
    throw input_error("Listening socket must be inet or inet6 address type");

  sa_unique_ptr try_address = sa_copy(bind_address);

  auto [stream_fd, datagram_fd] = listen_fd_open(bind_address);

  do {
    sa_set_port(try_address.get(), first);

    if (!fd_bind(stream_fd, try_address.get()))
      continue;

    // Try to bind the datagram socket to the same port to verify DHT availability.
    if (!fd_bind(datagram_fd, try_address.get())) {
      lt_log_print(LOG_CONNECTION_LISTEN, "failed to bind datagram socket on port %" PRIu64 " to verify DHT availability : %s",
                   first, std::strerror(errno));

      fd_close(stream_fd);
      fd_close(datagram_fd);

      std::tie(stream_fd, datagram_fd) = listen_fd_open(bind_address);
      continue;
    }

    fd_close(datagram_fd);

    if (!fd_listen(stream_fd, backlog)) {
      fd_close(stream_fd);
      throw resource_error("Could not listen on socket: " + std::string(strerror(errno)));
    }

    m_port = first;
    m_fileDesc = stream_fd;

    manager->connection_manager()->inc_socket_count();

    this_thread::poll()->open(this);
    this_thread::poll()->insert_read(this);
    this_thread::poll()->insert_error(this);

    lt_log_print(LOG_CONNECTION_LISTEN, "listen port %" PRIu64 " opened with backlog set to %i",
                 m_port, backlog);
    return true;

  } while (first++ < last);

  fd_close(stream_fd);
  fd_close(datagram_fd);

  lt_log_print(LOG_CONNECTION_LISTEN, "failed to find a suitable listen port : last error : %s",
               std::strerror(errno));
  return false;
}

void Listen::close() {
  if (m_fileDesc == -1)
    return;

  this_thread::poll()->remove_and_close(this);

  manager->connection_manager()->dec_socket_count();

  fd_close(m_fileDesc);

  m_fileDesc = -1;
  m_port = 0;
}

void
Listen::event_read() {
  while (true) {
    int fd;
    sa_unique_ptr sa;

    std::tie(fd, sa) = fd_sap_accept(get_fd().get_fd());

    if (fd == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;

      // Force a new event_read() call just to be sure we don't enter an infinite loop.
      if (errno == ECONNABORTED)
        break;

      throw resource_error("Listener port accept() failed: " + std::string(std::strerror(errno)));
    }

    m_slot_accepted(fd, sa.get());
  }
}

void
Listen::event_write() {
  throw internal_error("Listener does not support write().");
}

void
Listen::event_error() {
  int socket_error;

  if (!fd_get_socket_error(m_fileDesc, &socket_error))
    throw internal_error("Listener port could not get socket error: " + std::string(std::strerror(errno)));

  if (socket_error != 0)
    throw internal_error("Listener port received an error event: " + std::string(std::strerror(socket_error)));
}

} // namespace torrent
