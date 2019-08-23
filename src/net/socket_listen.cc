#include "config.h"

#include "socket_listen.h"

#include <algorithm>

#include "torrent/connection_manager.h"
#include "torrent/exceptions.h"
#include "torrent/utils/log.h"
#include "torrent/utils/random.h"

#define LT_LOG_SAP(log_fmt, sap, ...)                                   \
  lt_log_print(LOG_CONNECTION_LISTEN, "listen->%s: " log_fmt, sap_pretty_str(sap).c_str(), __VA_ARGS__);

namespace torrent {

socket_listen::socket_listen() : m_backlog(SOMAXCONN) {
}

void
socket_listen::set_backlog(int backlog) {
  if (backlog < 0 || backlog > SOMAXCONN)
    throw internal_error("Could not set socket_listen backlog, out-of-range value.");

  m_backlog = backlog;
}

bool
socket_listen::open(sa_unique_ptr&& sap, uint16_t first_port, uint16_t last_port, uint16_t start_port, fd_flags open_flags) {
  if (is_open())
    throw internal_error("socket_listen::open: already open");

  if (!(sap_is_inet(sap) || sap_is_inet6(sap)) || sap_is_v4mapped(sap) || !sap_is_port_any(sap) || sap_is_broadcast(sap))
    throw internal_error("socket_listen::open: socket address must be inet/inet6 with no port, and not v4mapped nor broadcast: " + sap_pretty_str(sap));

  if (sap_is_inet(sap) && !(open_flags & fd_flag_v4only))
    throw internal_error("socket_listen::open: socket address is inet without v4only flag");

  if (first_port == 0 || last_port == 0 || start_port == 0 ||
      !(first_port <= last_port && first_port <= start_port && start_port <= last_port))
    throw internal_error("socket_listen::open: port range not valid");

  int fd = fd_open(open_flags);

  if (fd == -1) {
    LT_LOG_SAP("open failed (flags:0x%x errno:%i message:'%s')", sap, open_flags, errno, std::strerror(errno));
    return false;
  }

  uint16_t p = start_port;

  do {
    if (m_open_port(fd, sap, p))
      return is_open();

    if (p == last_port)
      p = first_port;
    else
      p++;
  } while (p != start_port);

  LT_LOG_SAP("listen ports exhausted (fd:%i first_port:%" PRIu16 " last_port:%" PRIu16 ")",
             sap, fd, first_port, last_port);
  fd_close(fd);
  return false;
}

bool
socket_listen::open_randomize(sa_unique_ptr&& sap, uint16_t first_port, uint16_t last_port, fd_flags open_flags) {
  if (last_port < first_port)
    throw internal_error("socket_listen::open_randomize: port range not valid");

  return open(std::move(sap), first_port, last_port, random_uniform_uint16(first_port, last_port), open_flags);
}

bool
socket_listen::open_sequential(sa_unique_ptr&& sap, uint16_t first_port, uint16_t last_port, fd_flags open_flags) {
  return open(std::move(sap), first_port, last_port, first_port, open_flags);
}

void
socket_listen::close() {
  if (!is_open())
    return;

  torrent::poll_event_closed(this);

  fd_close(file_descriptor());
  set_file_descriptor(-1);
  m_socket_address.reset();
}

void
socket_listen::event_read() {
}

void
socket_listen::event_error() {
}

// Returns true if open is successful or if we cannot bind to the
// address, returns false if other ports can be used.
bool
socket_listen::m_open_port(int fd, sa_unique_ptr& sap, uint16_t port) {
  sap_set_port(sap, port);

  if (!fd_bind(fd, sap.get())) {
    if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
      LT_LOG_SAP("listen address not usable (fd:%i errno:%i message:'%s')",
                 sap, fd, errno, std::strerror(errno));
      fd_close(fd);
      return true;
    }

    return false;
  }

  if (!fd_listen(fd, m_backlog)) {
    LT_LOG_SAP("call to listen failed (fd:%i backlog:%i errno:%i message:'%s')",
               sap, fd, m_backlog, errno, std::strerror(errno));
    fd_close(fd);
    return true;
  }

  LT_LOG_SAP("open listen port success (fd:%i backlog:%i)", sap, fd, m_backlog);

  m_fileDesc = fd;
  m_socket_address.swap(sap);

  torrent::poll_event_open(this);
  torrent::poll_event_insert_read(this);
  torrent::poll_event_insert_error(this);

  return true;
}

}
