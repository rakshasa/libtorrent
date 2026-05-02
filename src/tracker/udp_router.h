#ifndef LIBTORRENT_TRACKER_UDP_ROUTER_H
#define LIBTORRENT_TRACKER_UDP_ROUTER_H

#include <functional>
#include <memory>
#include <unordered_map>

#include "net/protocol_buffer.h"
#include "net/socket_datagram.h"

namespace torrent::tracker {

class UdpRouter : public SocketDatagram {
public:
  using buffer_type  = ProtocolBuffer<512>;
  using prepare_func = std::function<void(buffer_type&)>;
  using process_func = std::function<bool(const buffer_type&)>;

  UdpRouter() = default;
  ~UdpRouter() = default;

  const char*         type_name() const override { return "tracker_udp_router"; }

  // TODO: Listen to network_config updates and reopen if necessary.

  void                open(int family);
  void                close();

  // TODO: Add option for single-try.

  uint32_t            connect(c_sa_shared_ptr address, prepare_func prepare_fn, process_func process_fn);
  void                disconnect(uint32_t connection_id);

private:
  struct connection_info {
    prepare_func prepare;
    process_func process;
  };

  using connection_map = std::unordered_map<uint32_t, connection_info>;

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

  connection_map      m_connections;

  // TODO: Set Event::m_socket_address.
  buffer_type         m_buffer;

};

} // namespace torrent::tracker

#endif // LIBTORRENT_TRACKER_UDP_ROUTER_H
