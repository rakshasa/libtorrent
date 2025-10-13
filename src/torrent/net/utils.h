#ifndef LIBTORRENT_TORRENT_NET_UTILS_H
#define LIBTORRENT_TORRENT_NET_UTILS_H

#include <torrent/common.h>
#include <torrent/net/types.h>

namespace torrent::net {

c_sa_shared_ptr lookup_address(const std::string& address_str, int family);

sin_unique_ptr  detect_local_sin_addr();
sin6_unique_ptr detect_local_sin6_addr();

} // namespace torrent::net

#endif // LIBTORRENT_TORRENT_NET_UTILS_H
