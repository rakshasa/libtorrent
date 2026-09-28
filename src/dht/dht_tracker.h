#ifndef LIBTORRENT_DHT_TRACKER_H
#define LIBTORRENT_DHT_TRACKER_H

#include <netinet/in.h>
#include <vector>

#include "net/address_list.h" // For SA.
#include "torrent/object_raw_bencode.h"

namespace torrent {

// Container for peers tracked in a torrent. The IPv4 router stores IPv4 peers, the IPv6 router (BEP 32) IPv6
// peers; each answers get_peers with its own family's values.

class DhtTracker {
public:
  // Maximum number of peers we return for a GET_PEERS query (default value only). 
  // Needs to be small enough so that a packet with a payload of num_peers*6 bytes 
  // does not need fragmentation. Value chosen so that the size is approximately
  // equal to a FIND_NODE reply (8*26 bytes). IPv6 values are 21 bytes each: 32 of them
  // still fit a 1232-byte IPv6 minimum-MTU payload with the rest of the reply.
  static constexpr unsigned int max_peers = 32;

  // Maximum number of peers we keep track of. For torrents with more peers,
  // we replace the oldest peer with each new announce to avoid excessively
  // large peer tables for very active torrents.
  static constexpr unsigned int max_size = 128;

  bool                empty() const                { return m_peers.empty() && m_peers6.empty(); }
  size_t              size() const                 { return m_peers.size() + m_peers6.size(); }

  void                add_peer(uint32_t addr_n, uint16_t port);
  raw_list            get_peers(unsigned int maxPeers = max_peers);

  void                add_peer6(const in6_addr& addr, uint16_t port);
  raw_list            get_peers6(unsigned int maxPeers = max_peers);

  // Remove old announces from the tracker that have not reannounced for
  // more than the given number of seconds.
  void                prune(uint32_t maxAge);

  // Time of the most recent announce, or zero if no peers are tracked.
  uint32_t            last_seen() const;

private:
  // We need to store the address as a bencoded string.
  struct [[gnu::packed]] BencodeAddress {
    char                 header[2];
    SocketAddressCompact peer;

    BencodeAddress(const SocketAddressCompact& p) : peer(p) { header[0] = '6'; header[1] = ':'; }

    const char*  bencode() const { return header; }

    bool         empty() const   { return !peer.port; }
    bool         same_addr(const BencodeAddress& o) const { return peer.addr == o.peer.addr; }
    void         set_port(const BencodeAddress& o)       { peer.port = o.peer.port; }
  };

  struct [[gnu::packed]] BencodeAddress6 {
    char                  header[3];
    SocketAddressCompact6 peer;

    BencodeAddress6(const SocketAddressCompact6& p) : peer(p) { header[0] = '1'; header[1] = '8'; header[2] = ':'; }

    const char*  bencode() const { return header; }

    bool         empty() const   { return !peer.port; }
    bool         same_addr(const BencodeAddress6& o) const;
    void         set_port(const BencodeAddress6& o)       { peer.port = o.peer.port; }
  };

  using PeerList  = std::vector<BencodeAddress>;
  using PeerList6 = std::vector<BencodeAddress6>;

  PeerList               m_peers;
  std::vector<uint32_t>  m_lastSeen;

  PeerList6              m_peers6;
  std::vector<uint32_t>  m_lastSeen6;
};

} // namespace torrent

#endif
