#ifndef LIBTORRENT_CONNECTION_MANAGER_H
#define LIBTORRENT_CONNECTION_MANAGER_H

#include <functional>
#include <list>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <torrent/common.h>

namespace torrent {

// Standard pair of up/down throttles.
// First element is upload throttle, second element is download throttle.
using ThrottlePair = std::pair<Throttle*, Throttle*>;

class LIBTORRENT_EXPORT ConnectionManager {
public:
  using size_type     = uint32_t;
  using port_type     = uint16_t;

  // TODO: Move.
  enum {
    handshake_incoming           = 1,
    handshake_outgoing           = 2,
    handshake_outgoing_encrypted = 3,
    handshake_outgoing_proxy     = 4,
    handshake_success            = 5,
    handshake_dropped            = 6,
    handshake_failed             = 7,
    handshake_retry_plaintext    = 8,
    handshake_retry_encrypted    = 9
  };

  using slot_filter_type   = std::function<uint32_t(const sockaddr*)>;
  using slot_throttle_type = std::function<ThrottlePair(const sockaddr*)>;

  ConnectionManager();
  ~ConnectionManager();
  ConnectionManager(const ConnectionManager&) = delete;
  ConnectionManager& operator=(const ConnectionManager&) = delete;

  uint32_t            filter(const sockaddr* sa);
  void                set_filter(const slot_filter_type& s)   { m_slot_filter = s; }

  // The slot returns a ThrottlePair to use for the given address, or
  // NULLs to use the default throttle.
  slot_throttle_type& address_throttle()  { return m_slot_address_throttle; }

private:
  slot_filter_type    m_slot_filter;
  slot_throttle_type  m_slot_address_throttle;
};

} // namespace torrent

#endif

