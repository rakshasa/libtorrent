#ifndef LIBTORRENT_NET_SOCKET_ADDRESS_H
#define LIBTORRENT_NET_SOCKET_ADDRESS_H

#include <string>
#include <torrent/common.h>

namespace torrent {

bool sa_is_bindable(const sockaddr* sockaddr) LIBTORRENT_EXPORT;
bool sa_is_default(const sockaddr* sockaddr) LIBTORRENT_EXPORT;

std::string sa_pretty_address_str(const sockaddr* sockaddr) LIBTORRENT_EXPORT;

}

#endif
