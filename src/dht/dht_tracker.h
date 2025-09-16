#ifndef LIBTORRENT_DHT_TRACKER_H
#define LIBTORRENT_DHT_TRACKER_H

#include <vector>

#include "net/address_list.h" // For SA.
#include "torrent/object_raw_bencode.h"

namespace torrent {

// Container for peers tracked in a torrent.

class DhtTracker {
public:
  // Maximum number of peers we return for a GET_PEERS query (default value only). 
  // Needs to be small enough so that a packet with a payload of num_peers*6 bytes 
  // does not need fragmentation. Value chosen so that the size is approximately
  // equal to a FIND_NODE reply (8*26 bytes).
  static constexpr unsigned int max_peers = 32;

  // Maximum number of peers we keep track of. For torrents with more peers,
  // we replace the oldest peer with each new announce to avoid excessively
  // large peer tables for very active torrents.
  static constexpr unsigned int max_size = 128;

  bool                empty() const                { return m_peers.empty(); }
  size_t              size() const                 { return m_peers.size(); }

  void                add_peer(uint32_t addr_n, uint16_t port);
  raw_list            get_peers(unsigned int maxPeers = max_peers);

  // Remove old announces from the tracker that have not reannounced for
  // more than the given number of seconds.
  void                prune(uint32_t maxAge);

private:
  // We need to store the address as a bencoded string.
  struct [[gnu::packed]] BencodeAddress {
    char                 header[2];
    SocketAddressCompact peer;

    BencodeAddress(const SocketAddressCompact& p) : peer(p) { header[0] = '6'; header[1] = ':'; }

    const char*  bencode() const { return header; }

    bool         empty() const   { return !peer.port; }
  };

  using PeerList = std::vector<BencodeAddress>;

  PeerList               m_peers;
  std::vector<uint32_t>  m_lastSeen;
};

} // namespace torrent

#endif
