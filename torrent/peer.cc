#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "peer.h"

namespace torrent {

Peer::Peer() :
  m_port(0),
  m_protocol("unknown"),
  m_options(std::string(8, 0))
{
}

Peer::Peer(const std::string& id, const std::string& dns, unsigned int port) :
  m_id(id),
  m_dns(dns),
  m_port(port)
{
}

Peer::~Peer() {
}

bool Peer::is_valid() const {
  return m_id.length() == 20 &&
    m_dns.length() &&
    m_port;
}

}
