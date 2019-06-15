#include "config.h"

#include "socket_listen.h"

#include "torrent/exceptions.h"
#include "torrent/utils/log.h"

#define LT_LOG_SAP(log_fmt, sap, ...)                                   \
  lt_log_print(LOG_CONNECTION_LISTEN, "listen->%s: " log_fmt, sap_pretty_str(sap).c_str(), __VA_ARGS__);

namespace torrent {

socket_listen::socket_listen() : m_backlog(SOMAXCONN) {
  m_fileDesc = -1;
}

socket_listen::~socket_listen() {
  close();
}

void
socket_listen::set_backlog(int backlog) {
  if (backlog < 0 || backlog > SOMAXCONN)
    throw internal_error("Could not set socket_listen backlog, out-of-range value.");

  m_backlog = backlog;
}

bool
socket_listen::open(sa_unique_ptr&& sap, uint16_t first_port, uint16_t last_port, uint16_t itr_port, fd_flags open_flags) {
  if (is_open())
    throw internal_error("socket_listen::open: already open");

  if (!(sap_is_inet(sap) || sap_is_inet6(sap)) || sap_is_v4mapped(sap) || !sap_is_port_any(sap) || sap_is_broadcast(sap))
    throw internal_error("socket_listen::open: socket address must be inet/inet6 with no port, and not v4mapped nor broadcast: " + sap_pretty_str(sap));

  if (sap_is_inet(sap) && !(open_flags & fd_flag_v4only))
    throw internal_error("socket_listen::open: socket address is inet without v4only flag");

  if (first_port == 0 || last_port == 0 || itr_port == 0 ||
      !(first_port <= last_port && first_port <= itr_port && itr_port <= last_port))
    throw internal_error("socket_listen::open: port range not valid");

  int fd = fd_open(open_flags);

  if (fd == -1) {
    LT_LOG_SAP("open failed (flags:0x%x errno:%i message:'%s')", sap, open_flags, errno, std::strerror(errno));
    return false;
  }

  uint16_t stop_port = itr_port;

  do {
    sap_set_port(sap, itr_port);

    if (!fd_bind(fd, sap.get())) {
      if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
        LT_LOG_SAP("listen address not usable (fd:%i errno:%i message:'%s')", sap, fd, errno, std::strerror(errno));
        fd_close(fd);
        return false;
      }

      if (itr_port == last_port)
        itr_port = first_port;
      else
        itr_port++;

      continue;
    }

    if (!fd_listen(fd, m_backlog)) {
      LT_LOG_SAP("call to listen failed (fd:%i backlog:%i errno:%i message:'%s')",
                 sap, fd, m_backlog, errno, std::strerror(errno));
      fd_close(fd);
      return false;
    }

    LT_LOG_SAP("listen success (fd:%i)", sap, fd);

    m_fileDesc = fd;
    m_socket_address.swap(sap);
    return true;

  } while (itr_port != stop_port);

  LT_LOG_SAP("listen ports exhausted (fd:%i first_port:%" PRIu16 " last_port:%" PRIu16 ")",
             sap, fd, first_port, last_port);
  fd_close(fd);
  return false;
}

void
socket_listen::close() {
  if (!is_open())
    return;

  fd_close(m_fileDesc);

  m_fileDesc = -1;
  m_socket_address.reset();
}

void
socket_listen::event_read() {
}

void
socket_listen::event_write() {
}

void
socket_listen::event_error() {
}

}
