#include "config.h"

#include "torrent/connection_manager.h"

#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/net/network_config.h"

namespace torrent {

ConnectionManager::ConnectionManager() = default;
ConnectionManager::~ConnectionManager() = default;

bool
ConnectionManager::can_connect() const {
  return m_size < m_maxSize;
}

uint32_t
ConnectionManager::filter(const sockaddr* sa) {
  // TODO: Reverse order of checks, NC should be last.
  if (config::network_config()->is_block_ipv4() && sa_is_inet(sa))
    return 0;

  if (config::network_config()->is_block_ipv6() && sa_is_inet6(sa))
    return 0;

  if (config::network_config()->is_block_ipv4in6() && sa_is_v4mapped(sa))
    return 0;

  if (m_slot_filter)
    return m_slot_filter(sa);

  return 1;
}

} // namespace torrent
