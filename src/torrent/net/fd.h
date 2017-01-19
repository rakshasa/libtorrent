#ifndef LIBTORRENT_NET_FD_H
#define LIBTORRENT_NET_FD_H

#include <string>
#include <torrent/common.h>

namespace torrent {

void fd_close(int fd) LIBTORRENT_EXPORT;

bool fd_bind(int fd, const sockaddr* sa, bool use_inet6 = true) LIBTORRENT_EXPORT;

}

#endif
