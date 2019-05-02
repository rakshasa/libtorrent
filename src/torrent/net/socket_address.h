#ifndef LIBTORRENT_NET_SOCKET_ADDRESS_H
#define LIBTORRENT_NET_SOCKET_ADDRESS_H

#include <memory>
#include <string>
#include <torrent/common.h>

namespace torrent {

// TODO: Rename sa_in* to sin*.
typedef std::unique_ptr<sockaddr> sa_unique_ptr;
typedef std::unique_ptr<sockaddr_in> sa_in_unique_ptr;
typedef std::unique_ptr<sockaddr_in6> sa_in6_unique_ptr;

bool sa_is_unspec(const sockaddr* sockaddr) LIBTORRENT_EXPORT;
bool sa_is_inet(const sockaddr* sockaddr) LIBTORRENT_EXPORT;
bool sa_is_inet6(const sockaddr* sockaddr) LIBTORRENT_EXPORT;
bool sa_is_inet_inet6(const sockaddr* sockaddr) LIBTORRENT_EXPORT;

bool sa_is_any(const sockaddr* sockaddr) LIBTORRENT_EXPORT;
bool sa_in_is_any(const sockaddr_in* sockaddr) LIBTORRENT_EXPORT;
bool sa_in6_is_any(const sockaddr_in6* sockaddr) LIBTORRENT_EXPORT;

bool sa_is_v4mapped(const sockaddr* sockaddr) LIBTORRENT_EXPORT;
bool sa_in6_is_v4mapped(const sockaddr_in6* sockaddr) LIBTORRENT_EXPORT;

size_t sa_length(const sockaddr* sa) LIBTORRENT_EXPORT;

sa_unique_ptr     sa_make_unspec() LIBTORRENT_EXPORT;
sa_unique_ptr     sa_make_inet() LIBTORRENT_EXPORT;
sa_unique_ptr     sa_make_inet6() LIBTORRENT_EXPORT;

sa_unique_ptr     sa_copy(const sockaddr* sa) LIBTORRENT_EXPORT;
sa_unique_ptr     sa_copy_inet(const sockaddr_in* sa) LIBTORRENT_EXPORT;
sa_unique_ptr     sa_copy_inet6(const sockaddr_in6* sa) LIBTORRENT_EXPORT;

sa_in_unique_ptr  sa_in_make() LIBTORRENT_EXPORT;
sa_in6_unique_ptr sa_in6_make() LIBTORRENT_EXPORT;

sa_in_unique_ptr  sa_in_copy(const sockaddr_in* sa) LIBTORRENT_EXPORT;
sa_in6_unique_ptr sa_in6_copy(const sockaddr_in6* sa) LIBTORRENT_EXPORT;

sa_unique_ptr     sa_from_v4mapped(const sockaddr* sa) LIBTORRENT_EXPORT;
sa_unique_ptr     sa_to_v4mapped(const sockaddr* sa) LIBTORRENT_EXPORT;

sa_in_unique_ptr  sa_in_from_in6_v4mapped(const sockaddr_in6* sa) LIBTORRENT_EXPORT;
sa_in6_unique_ptr sa_in6_to_in_v4mapped(const sockaddr_in* sa) LIBTORRENT_EXPORT;

sa_in_unique_ptr  sin_from_sa(sa_unique_ptr&& sap) LIBTORRENT_EXPORT;
sa_in6_unique_ptr sin6_from_sa(sa_unique_ptr&& sap) LIBTORRENT_EXPORT;

void        sa_clear_inet6(sockaddr_in6* sa) LIBTORRENT_EXPORT;

uint16_t    sa_port(const sockaddr* sa) LIBTORRENT_EXPORT;
void        sa_set_port(sockaddr* sa, uint16_t port) LIBTORRENT_EXPORT;
bool        sa_is_port_any(const sockaddr* sa) LIBTORRENT_EXPORT;

std::string sa_addr_str(const sockaddr* sockaddr) LIBTORRENT_EXPORT;
std::string sa_in_addr_str(const sockaddr_in* sockaddr) LIBTORRENT_EXPORT;
std::string sa_in6_addr_str(const sockaddr_in6* sockaddr) LIBTORRENT_EXPORT;

std::string sa_pretty_str(const sockaddr* sockaddr) LIBTORRENT_EXPORT;
std::string sa_in_pretty_str(const sockaddr_in* sockaddr) LIBTORRENT_EXPORT;
std::string sa_in6_pretty_str(const sockaddr_in6* sockaddr) LIBTORRENT_EXPORT;

// Rename/replace:
void sa_inet_mapped_inet6(const sockaddr_in* sa, sockaddr_in6* mapped) LIBTORRENT_EXPORT;

std::string sa_pretty_address_str(const sockaddr* sockaddr) LIBTORRENT_EXPORT;

}

#endif
