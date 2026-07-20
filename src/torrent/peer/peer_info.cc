#include "config.h"

#include "utils/instrumentation.h"

#include <cassert>

#include "torrent/exceptions.h"
#include "torrent/net/socket_address.h"
#include "torrent/peer/peer_info.h"

namespace torrent {

// TODO: Use a safer socket address parameter.
PeerInfo::PeerInfo(const sockaddr* address) {
  m_address = sa_copy(address);
}

PeerInfo::~PeerInfo() {
  // if (m_transferCounter != 0)
  //   throw internal_error("PeerInfo::~PeerInfo() m_transferCounter != 0.");

  instrumentation_update(INSTRUMENTATION_TRANSFER_PEER_INFO_UNACCOUNTED, m_transferCounter);

  assert(!is_blocked() && "PeerInfo::~PeerInfo() peer is blocked.");
}

void
PeerInfo::set_port(uint16_t port) {
  return sa_set_port(m_address.get(), port);
}

} // namespace torrent
