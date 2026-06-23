#ifndef LIBTORRENT_NET_SENDFILE_STREAM_H
#define LIBTORRENT_NET_SENDFILE_STREAM_H

#include <cstdint>

namespace torrent {

// Unified sendfile API: (socket_fd, file_fd, offset, length).
// Platform backends: Linux (4-arg), FreeBSD (7-arg), Darwin/macOS (6-arg, len in/out).
// Returns the number of bytes sent, or zero if the socket would block
uint32_t sendfile_stream(int socket_fd, int file_fd, uint64_t offset, uint32_t length);

} // namespace torrent

#endif
