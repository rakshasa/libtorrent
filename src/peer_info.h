#ifndef LIBTORRENT_PEER_INFO_H
#define LIBTORRENT_PEER_INFO_H

#include <string>
#include <inttypes.h>

namespace torrent {

class PeerInfo {
public:
  PeerInfo() :
    m_port(0), m_options(std::string(8, 0)) {}
  PeerInfo(const std::string& id, const std::string& dns, uint16_t port) :
    m_port(port), m_id(id), m_dns(dns) {}

  const std::string&  get_id() const                    { return m_id; }
  const std::string&  get_dns() const                   { return m_dns; }
  uint16_t            get_port() const                  { return m_port; }
  const std::string&  get_options() const               { return m_options; }

  void                set_id(const std::string& id)     { m_id = id; }
  void                set_dns(const std::string& dns)   { m_dns = dns; }
  void                set_port(uint16_t p)              { m_port = p; }
  void                set_options(const std::string& o) { m_options = o; }

  bool                is_valid() const;
  bool                is_same_host(const PeerInfo& p) const { return m_dns == p.m_dns && m_port == p.m_port; }

  bool operator < (const PeerInfo& p) const { return m_id < p.m_id; }
  bool operator == (const PeerInfo& p) const { return m_id == p.m_id; }

private:
  uint16_t    m_port;

  std::string m_id;
  std::string m_dns;
  std::string m_options;
};

} // namespace torrent

#endif // LIBTORRENT_PEER_INFO_H
