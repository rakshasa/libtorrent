#ifndef LIBTORRENT_TRACKER_UDP_ROUTER_H
#define LIBTORRENT_TRACKER_UDP_ROUTER_H

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <random>
#include <tuple>
#include <unordered_map>

#include "net/protocol_buffer.h"
#include "net/socket_datagram.h"
#include "torrent/utils/scheduler.h"

namespace torrent::tracker {

class UdpRouter : public SocketDatagram {
public:
  using buffer_type  = ProtocolBuffer<512>;

  using prepare_func = std::function<void(uint32_t, buffer_type&)>;
  using process_func = std::function<bool(uint32_t, buffer_type&)>;
  using failure_func = std::function<void(uint32_t, int, int)>;
  using update_func  = std::function<void(uint32_t)>;

  UdpRouter();
  ~UdpRouter();

  const char*         type_name() const override { return "udp_router"; }

  // TODO: Listen to network_config updates and reopen if necessary.

  void                open(int family);
  void                close();

  // TODO: Add option for single-try.

  // These may call prepare_fn with the new id before returning, except for when hostname lookup is required.
  uint32_t            connect(c_sa_shared_ptr address, prepare_func prepare_fn, process_func process_fn, failure_func failure_fn, update_func update_fn = nullptr);
  uint32_t            connect(const std::string hostname, uint16_t port, prepare_func prepare_fn, process_func process_fn, failure_func failure_fn);

  uint32_t            transfer(uint32_t id, prepare_func prepare_fn, process_func process_fn, failure_func failure_fn, update_func update_fn = nullptr);

  void                disconnect(uint32_t id);

private:
  UdpRouter(const UdpRouter&) = delete;
  UdpRouter& operator=(const UdpRouter&) = delete;

  struct connection_info;

  using random_engine      = std::independent_bits_engine<std::default_random_engine, 32, uint32_t>;
  using connection_map     = std::unordered_map<uint32_t, connection_info>;
  using write_queue_type   = std::deque<std::pair<uint32_t, connection_info*>>;
  using timeout_queue_type = std::deque<std::tuple<uint32_t, std::chrono::seconds, connection_info*>>;

  struct connection_info {
    c_sa_shared_ptr      address;

    prepare_func         prepare{};
    process_func         process{};
    failure_func         failure{};

    unsigned int         retry_count{};

    connection_info**    queue_ptr{};
    connection_info**    timeout_ptr{};
  };

  int                 router_family() const;

  connection_map::iterator connect_unsafe(c_sa_shared_ptr address, prepare_func prepare_fn, process_func process_fn, failure_func failure_fn);
  void                     disconnect_unsafe(connection_map::iterator itr);

  void                resolved_hostname(uint32_t id, uint16_t port, c_sin_shared_ptr& sin, int err, c_sin6_shared_ptr& sin6, int err6);

  bool                try_write(uint32_t id, connection_info* info);
  bool                do_write(uint32_t id, connection_info* info);
  void                queue_write(uint32_t id, connection_info* info);

  uint32_t            peek_transaction_id(buffer_type& buffer) const;

  void                receive_timeout();

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

  // TODO: Change Thread/Scheduler callbacks to use deque, and create callback handles.

  random_engine       m_random_engine;

  connection_map      m_connections;
  write_queue_type    m_write_queue;
  timeout_queue_type  m_timeout_queue;

  utils::SchedulerEntry m_task_timeout;
  buffer_type           m_buffer;
};

inline int UdpRouter::router_family() const { return socket_address()->sa_family; }

} // namespace torrent::tracker

#endif // LIBTORRENT_TRACKER_UDP_ROUTER_H
