#ifndef LIBTORRENT_NET_DNS_TYPES_H
#define LIBTORRENT_NET_DNS_TYPES_H

#include <string>

namespace torrent::net {

// TODO: Currently this is just a cache for the resolver, however we also want a cache so depending
// on how cleanly it can be implemented we might deprecate this intermediate class.

struct DnsKey {
  int         family;
  std::string hostname;

  bool operator==(const DnsKey& other) const {
    return family == other.family && hostname == other.hostname;
  }
};

} // namespace torrent::net

namespace std {

template <>
struct hash<torrent::net::DnsKey> {
  std::size_t operator()(const torrent::net::DnsKey& key) const {
    return std::hash<int>()(key.family) ^ std::hash<std::string>()(key.hostname);
  }
};

} // namespace std

#endif // LIBTORRENT_NET_DNS_TYPES_H
