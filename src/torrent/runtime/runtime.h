#ifndef LIBTORRENT_TORRENT_RUNTIME_RUNTIME_H
#define LIBTORRENT_TORRENT_RUNTIME_RUNTIME_H

#include <torrent/common.h>

namespace torrent::runtime {

bool                is_shutting_down() LIBTORRENT_EXPORT;
bool                is_quick_shutting_down() LIBTORRENT_EXPORT;

NetworkConfig*      network_config() LIBTORRENT_EXPORT;

NetworkManager*     network_manager() LIBTORRENT_EXPORT;
SocketManager*      socket_manager() LIBTORRENT_EXPORT;

const char*         version() LIBTORRENT_EXPORT;

uint16_t            listen_port() LIBTORRENT_EXPORT;


// Must be called from main thread:

// TODO: Move
void                dht_add_peer_node(const sockaddr* sa, uint16_t port) LIBTORRENT_EXPORT;

// TODO: Move to network/socket_manager?
uint32_t            total_handshakes() LIBTORRENT_EXPORT;

} // namespace torrent::runtime

#endif
