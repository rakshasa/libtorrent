#include "config.h"

#include "peer_info.h"

namespace torrent {

PeerInfo::PeerInfo() :
  m_port(0),
  m_protocol("unknown"),
  m_options(std::string(8, 0))
{
}

PeerInfo::PeerInfo(const std::string& id, const std::string& dns, unsigned int port) :
  m_id(id),
  m_dns(dns),
  m_port(port)
{
}

PeerInfo::~PeerInfo() {
}

bool PeerInfo::is_valid() const {
  return m_id.length() == 20 &&
    m_dns.length() &&
    m_port;
}

}
