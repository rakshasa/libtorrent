#include "config.h"

#include "dht_tracker.h"

#include <cstring>

namespace torrent {

namespace {

// The same bookkeeping for both families: refresh a known address, append while there is room, else replace
// the peer seen longest ago.
template <typename List, typename Entry>
void
tracker_add(List& peers, std::vector<uint32_t>& last_seen, const Entry& entry) {
  unsigned int oldest = 0;
  uint32_t minSeen = ~uint32_t();

  // Check if peer exists. If not, find oldest peer.
  for (unsigned int i = 0; i < peers.size(); i++) {
    if (peers[i].same_addr(entry)) {
      peers[i].set_port(entry);
      last_seen[i] = this_thread::cached_seconds().count();
      return;
    }

    if (last_seen[i] < minSeen) {
      minSeen = last_seen[i];
      oldest = i;
    }
  }

  // If peer doesn't exist, append to list if the table is not full.
  if (peers.size() < DhtTracker::max_size) {
    peers.emplace_back(entry);
    last_seen.push_back(this_thread::cached_seconds().count());
    return;
  }

  // Peer doesn't exist and table is full: replace oldest peer.
  peers[oldest] = entry;
  last_seen[oldest] = this_thread::cached_seconds().count();
}

// Return compact info as bencoded strings for up to maxPeers peers, returning different peers for each call
// if there are more.
template <typename List>
raw_list
tracker_get(List& peers, unsigned int maxPeers) {
  if (peers.empty())
    return raw_list();

  auto first = peers.begin();
  auto last  = peers.end();

  // If we have more than max_peers, randomly return block of peers.
  // The peers in overlapping blocks get picked twice as often, but
  // that's better than returning fewer peers.
  if (peers.size() > maxPeers) {
    unsigned int blocks = (peers.size() + maxPeers - 1) / maxPeers;

    first += (random() % blocks) * (peers.size() - maxPeers) / (blocks - 1);
    last = first + maxPeers;
  }

  return raw_list(first->bencode(), (last - first) * sizeof(*first));
}

template <typename List>
void
tracker_prune(List& peers, std::vector<uint32_t>& last_seen, uint32_t minSeen) {
  for (unsigned int i = 0; i < last_seen.size(); i++)
    if (last_seen[i] < minSeen) peers[i].peer.port = 0;

  peers.erase(std::remove_if(peers.begin(), peers.end(), [](const auto& p) { return p.empty(); }), peers.end());

  last_seen.erase(std::remove_if(last_seen.begin(),
                                 last_seen.end(),
                                 [minSeen](auto seen) { return seen < minSeen; }),
                  last_seen.end());

  if (peers.size() != last_seen.size())
    throw internal_error("DhtTracker::prune did inconsistent peer pruning.");
}

} // namespace

bool
DhtTracker::BencodeAddress6::same_addr(const BencodeAddress6& o) const {
  return std::memcmp(&peer.addr, &o.peer.addr, sizeof(peer.addr)) == 0;
}

void
DhtTracker::add_peer(uint32_t addr_n, uint16_t port) {
  if (port == 0)
    return;

  tracker_add(m_peers, m_lastSeen, BencodeAddress(SocketAddressCompact(addr_n, htons(port))));
}

void
DhtTracker::add_peer6(const in6_addr& addr, uint16_t port) {
  if (port == 0)
    return;

  tracker_add(m_peers6, m_lastSeen6, BencodeAddress6(SocketAddressCompact6(addr, htons(port))));
}

// 8 bytes per IPv4 peer ("6:" + address + port).
raw_list
DhtTracker::get_peers(unsigned int maxPeers) {
  if (sizeof(BencodeAddress) != 8)
    throw internal_error("DhtTracker::BencodeAddress is packed incorrectly.");

  return tracker_get(m_peers, maxPeers);
}

// 21 bytes per IPv6 peer ("18:" + address + port).
raw_list
DhtTracker::get_peers6(unsigned int maxPeers) {
  if (sizeof(BencodeAddress6) != 21)
    throw internal_error("DhtTracker::BencodeAddress6 is packed incorrectly.");

  return tracker_get(m_peers6, maxPeers);
}

// Remove old announces.
void
DhtTracker::prune(uint32_t maxAge) {
  uint32_t minSeen = this_thread::cached_seconds().count() - maxAge;

  tracker_prune(m_peers, m_lastSeen, minSeen);
  tracker_prune(m_peers6, m_lastSeen6, minSeen);
}

// Both families: the IPv6 router's trackers hold IPv6 peers only (m_lastSeen empty), and reading m_lastSeen
// alone would age every one of them to zero and evict in map order.
uint32_t
DhtTracker::last_seen() const {
  uint32_t seen = 0;

  if (!m_lastSeen.empty())
    seen = *std::max_element(m_lastSeen.begin(), m_lastSeen.end());

  if (!m_lastSeen6.empty())
    seen = std::max(seen, *std::max_element(m_lastSeen6.begin(), m_lastSeen6.end()));

  return seen;
}

} // namespace torrent
