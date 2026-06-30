#ifndef LIBTORRENT_TORRENT_RUNTIME_RUNTIME_H
#define LIBTORRENT_TORRENT_RUNTIME_RUNTIME_H

#include <torrent/common.h>

namespace RTORRENT_EXPORT torrent {

namespace runtime {

class ProxyManager;

bool                is_shutting_down();
bool                is_quick_shutting_down();

void                shutdown();
void                quick_shutdown();

NetworkConfig*      network_config();

NetworkManager*     network_manager();
SocketManager*      socket_manager();
ProxyManager*       proxy_manager();

const char*         version();

uint16_t            listen_port();


// Must be called from main thread:

// TODO: Move
void                dht_add_peer_node(const sockaddr* sa, uint16_t port);

// TODO: Move to network/socket_manager?
uint32_t            total_handshakes();

} // namespace torrent::runtime

} // namespace torrent

#endif
