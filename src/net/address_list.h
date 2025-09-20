#ifndef LIBTORRENT_DOWNLOAD_ADDRESS_LIST_H
#define LIBTORRENT_DOWNLOAD_ADDRESS_LIST_H

#include <string>
#include <vector>

#include "torrent/object.h"
#include "torrent/object_raw_bencode.h"
#include "torrent/net/types.h"

namespace torrent {

class AddressList : public std::vector<sa_inet_union> {
public:
  void                sort();
  void                sort_and_unique();

  // Parse normal or compact list of addresses and add to AddressList
  void                parse_address_normal(const Object::list_type& b);
  void                parse_address_bencode(raw_list s);

  void                parse_address_compact(raw_string s);
  void                parse_address_compact(const std::string& s);
  void                parse_address_compact_ipv6(const std::string& s);
};

inline void
AddressList::parse_address_compact(const std::string& s) {
  return parse_address_compact(raw_string(s.data(), s.size()));
}

// Move somewhere else.
struct [[gnu::packed]] SocketAddressCompact {
  SocketAddressCompact() = default;
  SocketAddressCompact(uint32_t a, uint16_t p) : addr(a), port(p) {}
  SocketAddressCompact(const sockaddr_in* sin) : addr(sin->sin_addr.s_addr), port(sin->sin_port) {}

  operator sa_inet_union () const {
    sa_inet_union su{};
    su.inet.sin_family = AF_INET;
    su.inet.sin_addr.s_addr = addr;
    su.inet.sin_port = port;

    return su;
  }

  uint32_t addr;
  uint16_t port;

  const char*         c_str() const { return reinterpret_cast<const char*>(this); }
  std::string         str() const   { return std::string(c_str(), sizeof(*this)); }
};

struct [[gnu::packed]] SocketAddressCompact6 {
  SocketAddressCompact6() = default;
  SocketAddressCompact6(in6_addr a, uint16_t p) : addr(a), port(p) {}

  operator sa_inet_union () const {
    sa_inet_union su{};
    su.inet6.sin6_family = AF_INET6;
    su.inet6.sin6_addr = addr;
    su.inet6.sin6_port = port;

    return su;
  }

  in6_addr addr;
  uint16_t port;

  const char*         c_str() const { return reinterpret_cast<const char*>(this); }
};

} // namespace torrent

#endif
