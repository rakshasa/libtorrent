#include "config.h"

#define __STDC_FORMAT_MACROS

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <rak/socket_address.h>
#include <sys/socket.h>

#include "torrent/exceptions.h"
#include "torrent/connection_manager.h"
#include "torrent/poll.h"
#include "torrent/utils/log.h"

#include "listen.h"
#include "manager.h"

namespace torrent {

bool
Listen::open(uint16_t first, uint16_t last, int backlog, const rak::socket_address* bindAddress) {
  close();

  if (first == 0 || first > last)
    throw input_error("Tried to open listening port with an invalid range.");

  if (bindAddress->family() != 0 &&
      bindAddress->family() != rak::socket_address::af_inet &&
      bindAddress->family() != rak::socket_address::af_inet6)
    throw input_error("Listening socket must be bound to an inet or inet6 address.");

  if (!get_fd().open_stream() ||
      !get_fd().set_nonblock() ||
      !get_fd().set_reuse_address(true))
    throw resource_error("Could not allocate socket for listening.");

  rak::socket_address sa;

  // TODO: Temporary until we refactor:
  if (bindAddress->family() == 0) {
    if (m_ipv6_socket)
      sa.sa_inet6()->clear();
    else
      sa.sa_inet()->clear();
  } else {
    sa.copy(*bindAddress, bindAddress->length());
  }

  do {
    sa.set_port(first);

    if (get_fd().bind(sa) && get_fd().listen(backlog)) {
      m_port = first;

      manager->connection_manager()->inc_socket_count();

      manager->poll()->open(this);
      manager->poll()->insert_read(this);
      manager->poll()->insert_error(this);

      lt_log_print(LOG_CONNECTION_LISTEN, "listen port %" PRIu16 " opened with backlog set to %i",
                   m_port, backlog);

      return true;

    }
  } while (first++ < last);

  // This needs to be done if local_error is thrown too...
  get_fd().close();
  get_fd().clear();

  lt_log_print(LOG_CONNECTION_LISTEN, "failed to open listen port");

  return false;
}

void Listen::close() {
  if (!get_fd().is_valid())
    return;

  manager->poll()->remove_read(this);
  manager->poll()->remove_error(this);
  manager->poll()->close(this);

  manager->connection_manager()->dec_socket_count();

  get_fd().close();
  get_fd().clear();
  
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

}
