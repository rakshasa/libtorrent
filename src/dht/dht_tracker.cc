#include "config.h"

#include "dht_tracker.h"

namespace torrent {

void
DhtTracker::add_peer(uint32_t addr_n, uint16_t port) {
  if (port == 0)
    return;

  SocketAddressCompact compact(addr_n, port);

  unsigned int oldest = 0;
  uint32_t minSeen = ~uint32_t();

  // Check if peer exists. If not, find oldest peer.
  for (unsigned int i = 0; i < size(); i++) {
    if (m_peers[i].peer.addr == compact.addr) {
      m_peers[i].peer.port = compact.port;
      m_lastSeen[i] = this_thread::cached_seconds().count();
      return;
    }

    if (m_lastSeen[i] < minSeen) {
      minSeen = m_lastSeen[i];
      oldest = i;
    }
  }

  // If peer doesn't exist, append to list if the table is not full.
  if (size() < max_size) {
    m_peers.emplace_back(compact);
    m_lastSeen.push_back(this_thread::cached_seconds().count());
    return;
  }

  // Peer doesn't exist and table is full: replace oldest peer.
  m_peers[oldest] = compact;
  m_lastSeen[oldest] = this_thread::cached_seconds().count();
}

// Return compact info as bencoded string (8 bytes per peer) for up to 30 peers,
// returning different peers for each call if there are more.
raw_list
DhtTracker::get_peers(unsigned int maxPeers) {
  if (sizeof(BencodeAddress) != 8)
    throw internal_error("DhtTracker::BencodeAddress is packed incorrectly.");

  auto first = m_peers.begin();
  auto last  = m_peers.end();

  // If we have more than max_peers, randomly return block of peers.
  // The peers in overlapping blocks get picked twice as often, but
  // that's better than returning fewer peers.
  if (m_peers.size() > maxPeers) {
    unsigned int blocks = (m_peers.size() + maxPeers - 1) / maxPeers;

    first += (random() % blocks) * (m_peers.size() - maxPeers) / (blocks - 1);
    last = first + maxPeers;
  }

  return raw_list(first->bencode(), last->bencode() - first->bencode());
}

// Remove old announces.
void
DhtTracker::prune(uint32_t maxAge) {
  uint32_t minSeen = this_thread::cached_seconds().count() - maxAge;

  for (unsigned int i = 0; i < m_lastSeen.size(); i++)
    if (m_lastSeen[i] < minSeen) m_peers[i].peer.port = 0;

  m_peers.erase(std::remove_if(m_peers.begin(),
                               m_peers.end(),
                               std::mem_fn(&BencodeAddress::empty)),
                m_peers.end());

  m_lastSeen.erase(std::remove_if(m_lastSeen.begin(),
                                  m_lastSeen.end(),
                                  [minSeen](auto seen) { return seen < minSeen; }),
                   m_lastSeen.end());

  if (m_peers.size() != m_lastSeen.size())
    throw internal_error("DhtTracker::prune did inconsistent peer pruning.");
}

} // namespace torrent
