#ifndef LIBTORRENT_PEER_INFO_H
#define LIBTORRENT_PEER_INFO_H

#include <string>

namespace torrent {

class PeerInfo {
public:
  PeerInfo();
  PeerInfo(const std::string& id, const std::string& dns, unsigned int port);
  ~PeerInfo();

  const std::string& id() const { return m_id; }
  const std::string& dns() const { return m_dns; }
  unsigned short port() const { return m_port; }

  const std::string& protocol() const { return m_protocol; }
  const std::string& options() const { return m_options; }

  bool is_valid() const;

  bool is_same_host(const PeerInfo& p) const { return m_dns == p.m_dns && m_port == p.m_port; }

  bool operator < (const PeerInfo& p) const { return m_id < p.m_id; }
  bool operator == (const PeerInfo& p) const { return m_id == p.m_id; }

  // Use only when required.
  std::string&    ref_id() { return m_id; }
  std::string&    ref_dns() { return m_dns; }
  unsigned short& ref_port() { return m_port; }
  std::string&    ref_protocol() { return m_protocol; }
  std::string&    ref_options() { return m_options; }

private:
  std::string m_id;

  std::string m_dns;
  unsigned short m_port;

  std::string m_protocol;
  std::string m_options;
};

} // namespace torrent

#endif // LIBTORRENT_PEER_INFO_H
