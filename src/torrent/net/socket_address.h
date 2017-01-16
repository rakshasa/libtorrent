#ifndef LIBTORRENT_NET_SOCKET_ADDRESS_H
#define LIBTORRENT_NET_SOCKET_ADDRESS_H

#include <string>
#include <torrent/common.h>

namespace torrent {

bool sa_is_bindable(const sockaddr* sockaddr) LIBTORRENT_EXPORT;
bool sa_is_default(const sockaddr* sockaddr) LIBTORRENT_EXPORT;

size_t sa_length(const sockaddr* sa) LIBTORRENT_EXPORT;

std::string sa_pretty_address_str(const sockaddr* sockaddr) LIBTORRENT_EXPORT;

void sa_inet6_clear(sockaddr_in6* sa) LIBTORRENT_EXPORT;

void sa_inet_mapped_inet6(const sockaddr_in* sa, sockaddr_in6* mapped) LIBTORRENT_EXPORT;

}

#endif
