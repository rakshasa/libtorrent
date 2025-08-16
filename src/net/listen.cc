#include "config.h"

#include "listen.h"

#include "manager.h"
#include "rak/socket_address.h"
#include "torrent/connection_manager.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/log.h"

namespace torrent {

bool
Listen::open(uint16_t first, uint16_t last, int backlog, const rak::socket_address* bindAddress) {
  close();

  if (first == 0 || first > last)
    throw input_error("Tried to open listening port with an invalid range.");

  auto bind_address = bindAddress->c_sockaddr();

  if (bind_address->sa_family != AF_UNSPEC &&
      bind_address->sa_family != AF_INET &&
      bind_address->sa_family != AF_INET6)
    throw input_error("Listening socket must be unspec, inet or inet6.");

  int fd = fd_open(fd_flag_stream | fd_flag_nonblock | fd_flag_reuse_address);

  if (fd == -1)
    throw resource_error("Could not open socket for listening: " + std::string(strerror(errno)));

  sa_unique_ptr try_address;

  if (bind_address->sa_family != AF_UNSPEC)
    try_address = sa_copy(bind_address);
  else if (m_ipv6_socket)
    try_address = sa_make_inet6_any();
  else
    try_address = sa_make_inet_any();

  // bool failed_to_bind_udp{};

  do {
    sa_set_port(try_address.get(), first);

    if (!fd_bind(fd, try_address.get()))
      continue;

    // TODO: Quick try to bind udp socket here.

    if (!fd_listen(fd, backlog)) {
      fd_close(fd);
      throw resource_error("Could not listen on socket: " + std::string(strerror(errno)));
    }

    m_port = first;
    m_fileDesc = fd;

    manager->connection_manager()->inc_socket_count();

    this_thread::poll()->open(this);
    this_thread::poll()->insert_read(this);
    this_thread::poll()->insert_error(this);

    lt_log_print(LOG_CONNECTION_LISTEN, "listen port %" PRIu64 " opened with backlog set to %i",
                 m_port, backlog);
    return true;

  } while (first++ < last);

  // This needs to be done if local_error is thrown too...
  fd_close(fd);

  lt_log_print(LOG_CONNECTION_LISTEN, "failed to find a suitable listen port");
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
  rak::socket_address sa;
  SocketFd fd;

  while ((fd = get_fd().accept(&sa)).is_valid())
    m_slot_accepted(fd, sa);
}

void
Listen::event_write() {
  throw internal_error("Listener does not support write().");
}

void
Listen::event_error() {
  int error = get_fd().get_error();

  if (error != 0)
    throw internal_error("Listener port received an error event: " + std::string(std::strerror(error)));
}

} // namespace torrent
