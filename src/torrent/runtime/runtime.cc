#include "config.h"

#include "torrent/runtime/runtime.h"

#include "manager.h"
#include "protocol/handshake_manager.h"
#include "torrent/runtime/network_manager.h"

namespace torrent::runtime {

const char*      version()                                            { return VERSION; }

uint16_t         listen_port()                                        { return network_manager()->listen_port(); }

void             dht_add_peer_node(const sockaddr* sa, uint16_t port) { network_manager()->dht_add_peer_node(sa, port); }

uint32_t         total_handshakes()                                   { return manager->handshake_manager()->size(); }

} // namespace torrent::runtime
