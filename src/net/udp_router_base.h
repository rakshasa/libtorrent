#ifndef LIBTORRENT_NET_UDP_ROUTER_BASE_H
#define LIBTORRENT_NET_UDP_ROUTER_BASE_H

#include <deque>
#include <functional>
#include <list>
#include <random>
#include <unordered_map>

#include "net/protocol_buffer.h"
#include "net/socket_datagram.h"
#include "torrent/system/scheduler.h"
#include "torrent/utils/list_buckets.h"

namespace torrent::net {

template <typename Info>
class UdpRouterBase : public SocketDatagram {
public:
  using key_type = Info::key_type;
};

} // namespace torrent::dht

#endif
