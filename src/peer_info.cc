#include "config.h"

#include "peer_info.h"

namespace torrent {

bool PeerInfo::is_valid() const {
  return m_id.length() == 20 &&
    m_dns.length() &&
    m_port;
}

}
