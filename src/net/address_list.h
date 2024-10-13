#ifndef LIBTORRENT_DOWNLOAD_ADDRESS_LIST_H
#define LIBTORRENT_DOWNLOAD_ADDRESS_LIST_H

#include <list>
#include <string>
#include <rak/socket_address.h>

#include <torrent/object.h>
#include <torrent/object_raw_bencode.h>

namespace torrent {

class AddressList : public std::list<rak::socket_address> {
public:
  // Parse normal or compact list of addresses and add to AddressList
  void                        parse_address_normal(const Object::list_type& b);
  void                        parse_address_bencode(raw_list s);

  void                        parse_address_compact(raw_string s);
  void                        parse_address_compact(const std::string& s);
  void                        parse_address_compact_ipv6(const std::string& s);
};

inline void
AddressList::parse_address_compact(const std::string& s) {
  return parse_address_compact(raw_string(s.data(), s.size()));
}

// Move somewhere else.
struct SocketAddressCompact {
  SocketAddressCompact() {}
  SocketAddressCompact(uint32_t a, uint16_t p) : addr(a), port(p) {}
  SocketAddressCompact(const rak::socket_address_inet* sa) : addr(sa->address_n()), port(sa->port_n()) {}

  operator rak::socket_address () const {
    rak::socket_address sa;
    sa.sa_inet()->clear();
    sa.sa_inet()->set_port_n(port);
    sa.sa_inet()->set_address_n(addr);

    return sa;
  }

  uint32_t addr;
  uint16_t port;

  const char*         c_str() const { return reinterpret_cast<const char*>(this); }
} __attribute__ ((packed));

struct SocketAddressCompact6 {
  SocketAddressCompact6() {}
  SocketAddressCompact6(in6_addr a, uint16_t p) : addr(a), port(p) {}
  SocketAddressCompact6(const rak::socket_address_inet6* sa) : addr(sa->address()), port(sa->port_n()) {}

  operator rak::socket_address () const {
    rak::socket_address sa;
    sa.sa_inet6()->clear();
    sa.sa_inet6()->set_port_n(port);
    sa.sa_inet6()->set_address(addr);

    return sa;
  }

  in6_addr addr;
  uint16_t port;

  const char*         c_str() const { return reinterpret_cast<const char*>(this); }
} __attribute__ ((packed));

}

#endif
