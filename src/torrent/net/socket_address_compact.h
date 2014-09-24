// Copyright (C) 2005-2014, Jari Sundell
// All rights reserved.

#ifndef LIBTORRENT_UTILS_SOCKET_ADDRESS_COMPACT_H
#define LIBTORRENT_UTILS_SOCKET_ADDRESS_COMPACT_H

// Unique key for the socket address, excluding port numbers, etc.

// TODO: Add include files...

#include <rak/socket_address.h>

namespace torrent {

struct socket_address_compact {
  socket_address_compact() {}
  socket_address_compact(uint32_t a, uint16_t p) : addr(a), port(p) {}
  socket_address_compact(const rak::socket_address_inet* sa) : addr(sa->address_n()), port(sa->port_n()) {}

  operator rak::socket_address () const {
    rak::socket_address sa;
    sa.sa_inet()->clear();
    sa.sa_inet()->set_port_n(port);
    sa.sa_inet()->set_address_n(addr);

    return sa;
  }

  uint32_t addr;
  uint16_t port;

  // TODO: c_str? should be c_ptr or something.
  const char*         c_str() const { return reinterpret_cast<const char*>(this); }
} __attribute__ ((packed));

struct socket_address_compact6 {
  socket_address_compact6() {}
  socket_address_compact6(in6_addr a, uint16_t p) : addr(a), port(p) {}
  socket_address_compact6(const rak::socket_address_inet6* sa) : addr(sa->address()), port(sa->port_n()) {}

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
