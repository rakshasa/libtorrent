#ifndef LIBTORRENT_TRACKER_UDP_ROUTER_H
#define LIBTORRENT_TRACKER_UDP_ROUTER_H

#include <functional>
#include <list>
#include <memory>
#include <random>
#include <unordered_map>

#include "net/protocol_buffer.h"
#include "net/socket_datagram.h"

namespace torrent::tracker {

class UdpRouter : public SocketDatagram {
public:
  using buffer_type  = ProtocolBuffer<512>;
  using prepare_func = std::function<void(uint32_t, buffer_type&)>;
  using process_func = std::function<bool(uint32_t, const buffer_type&)>;

  UdpRouter();
  ~UdpRouter() = default;

  const char*         type_name() const override { return "tracker_udp_router"; }

  // TODO: Listen to network_config updates and reopen if necessary.

  void                open(int family);
  void                close();

  // TODO: Add option for single-try.

  uint32_t            connect(c_sa_shared_ptr address, prepare_func prepare_fn, process_func process_fn);
  void                disconnect(uint32_t id);

private:
  struct connection_info;

  using connection_map = std::unordered_map<uint32_t, connection_info>;
  using queue_type     = std::list<std::pair<uint32_t, connection_info>>;
  using random_engine  = std::independent_bits_engine<std::default_random_engine, 32, uint32_t>;

  struct connection_info {
    c_sa_shared_ptr      address;
    prepare_func         prepare;
    process_func         process;

    queue_type::iterator queue_itr;
  };

  bool                try_write(uint32_t id, const connection_info& info);
  void                queue_write(uint32_t id, connection_info& info);

  uint32_t            peek_transaction_id(buffer_type& buffer) const;

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

  // TODO: Add timeout queue.

  connection_map      m_connections;
  queue_type          m_write_queue;
  random_engine       m_random_engine;

  // TODO: Set Event::m_socket_address.
  buffer_type         m_buffer;

};

} // namespace torrent::tracker

#endif // LIBTORRENT_TRACKER_UDP_ROUTER_H
