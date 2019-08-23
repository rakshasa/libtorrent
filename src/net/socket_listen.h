#ifndef LIBTORRENT_SOCKET_LISTEN_H
#define LIBTORRENT_SOCKET_LISTEN_H

#include <cinttypes>
#include <functional>

#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"
#include "torrent/net/socket_event.h"

namespace torrent {

class socket_listen : public socket_event {
public:
  typedef std::function<void (int, sa_unique_ptr)> accepted_ftor;

  socket_listen();

  int  backlog() const;

  void set_backlog(int backlog);
  void set_slot_accepted(accepted_ftor&& ftor);

  bool open(sa_unique_ptr&& sap, uint16_t first_port, uint16_t last_port, uint16_t start_port, fd_flags open_flags);
  bool open_randomize(sa_unique_ptr&& sap, uint16_t first_port, uint16_t last_port, fd_flags open_flags);
  bool open_sequential(sa_unique_ptr&& sap, uint16_t first_port, uint16_t last_port, fd_flags open_flags);
  void close();

  void event_read() override;
  void event_error() override;

  const char* type_name() const override { return "socket_listen"; }

private:
  bool m_open_port(int fd, sa_unique_ptr& sap, uint16_t port);

  int           m_backlog;
  accepted_ftor m_slot_accepted;
};

inline int  socket_listen::backlog() const { return m_backlog; }
inline void socket_listen::set_slot_accepted(accepted_ftor&& ftor) { m_slot_accepted = ftor; }

}

#endif
