#ifndef LIBTORRENT_SOCKET_LISTEN_H
#define LIBTORRENT_SOCKET_LISTEN_H

#include <cinttypes>
#include <functional>

#include "torrent/event.h"
#include "torrent/net/fd.h"
#include "torrent/net/socket_address.h"

namespace torrent {

class socket_listen : public Event {
public:
  // TODO: Add bind name, pass through bind_manager.
  typedef std::function<void (int, sa_unique_ptr)> accepted_ftor;

  socket_listen();
  virtual ~socket_listen();

  bool is_open() const;

  int             backlog() const;
  uint16_t        port() const;
  const sockaddr* socket_address() const;

  void set_backlog(int backlog);
  void set_slot_accepted(accepted_ftor&& ftor);

  bool open(sa_unique_ptr&& sap, uint16_t first_port, uint16_t last_port, uint16_t itr_port, fd_flags open_flags);
  void close();

  virtual void    event_read();
  virtual void    event_write();
  virtual void    event_error();

  virtual const char* type_name() const { return "listen"; }

private:
  // TODO: Move to Event.
  sa_unique_ptr m_socket_address;
  int           m_backlog;
  accepted_ftor m_slot_accepted;
};

inline bool            socket_listen::is_open() const { return m_fileDesc != -1; }
inline int             socket_listen::backlog() const { return m_backlog; }
inline uint16_t        socket_listen::port() const { return sap_port(m_socket_address); }
inline const sockaddr* socket_listen::socket_address() const { return m_socket_address.get(); }
inline void            socket_listen::set_slot_accepted(accepted_ftor&& ftor) { m_slot_accepted = ftor; }

}

#endif
