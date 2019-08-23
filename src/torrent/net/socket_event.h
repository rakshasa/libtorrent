#ifndef LIBTORRENT_SOCKET_EVENT_H
#define LIBTORRENT_SOCKET_EVENT_H

#include <cinttypes>

#include "torrent/event.h"
#include "torrent/net/socket_address.h"

namespace torrent {

class LIBTORRENT_EXPORT socket_event : public Event {
public:
  ~socket_event() override;

  const sockaddr* socket_address() const;
  uint16_t        socket_address_port() const;

  void event_read() override;
  void event_write() override;
  void event_error() override;

protected:
  sa_unique_ptr m_socket_address;
};

inline const sockaddr* socket_event::socket_address() const { return m_socket_address.get(); }
inline uint16_t        socket_event::socket_address_port() const { return sap_port(m_socket_address); }

}

#endif
