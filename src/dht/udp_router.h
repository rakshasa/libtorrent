#ifndef LIBTORRENT_DHT_UDP_ROUTER_H
#define LIBTORRENT_DHT_UDP_ROUTER_H

#include <deque>
#include <functional>
#include <list>
#include <random>
#include <unordered_map>

#include "net/protocol_buffer.h"
#include "net/socket_datagram.h"
#include "torrent/system/scheduler.h"

namespace torrent::dht {

class UdpRouter : public SocketDatagram {
public:
  using key_type         = uint64_t;
  using buffer_type      = ProtocolBuffer<2048>;
  using random_engine    = std::independent_bits_engine<std::mt19937, 64, key_type>;

  using unknown_func     = std::function<void(key_type, sockaddr*, buffer_type&)>;

  using prepare_func     = std::function<void(key_type, buffer_type&)>;
  using process_func     = std::function<bool(key_type, buffer_type&)>;
  using failure_func     = std::function<void(key_type, int, int)>;
  using packet_sent_func = std::function<void(key_type)>;

  struct connection_params {
    prepare_func        prepare;
    process_func        process;
    failure_func        failure;

    packet_sent_func    packet_sent;
  };

  UdpRouter();
  ~UdpRouter();

  const char*         type_name() const override { return "dht_udp_router"; }

  bool                has_connection_id(key_type id) const;

  void                set_unknown_callback(unknown_func fn);

  // TODO: Listen to network_config updates and reopen if necessary.
  // TODO: Add callback subscription for network_config updates.

  void                open(int family, uint16_t port);
  void                close();

  void                updated_network_config(int family);

  // TODO: Add option for single-try.

  // These may call prepare_fn with the new id before returning, except for when hostname lookup is
  // required.
  //
  // When process_fn is called, return false to disconnect and true to do nothing. (E.g. unknown
  // packet or transferred connection)

  void                connect(key_type id, c_sa_shared_ptr address, connection_params params);

  void                disconnect(key_type id);
  void                try_disconnect(key_type id);

private:
  UdpRouter(const UdpRouter&) = delete;
  UdpRouter& operator=(const UdpRouter&) = delete;

  static constexpr unsigned int max_read_datagrams = 64;

  struct connection_info;

  using connection_map     = std::unordered_map<key_type, connection_info>;
  using write_queue_type   = std::deque<std::pair<key_type, connection_info*>>;
  using timeout_queue_type = std::list<std::tuple<key_type, std::chrono::seconds, connection_info*>>;

  // TODO: Add itr to self in connection_map.

  struct connection_info {
    c_sa_shared_ptr      address;

    prepare_func         prepare{};
    process_func         process{};
    failure_func         failure{};

    // Do not change the connection in this callback.
    packet_sent_func     packet_sent{};

    unsigned int         retry_count{};

    connection_info**    queue_ptr{};
    connection_info**    timeout_ptr{};
  };

  connection_map::iterator connect_unsafe(key_type id, c_sa_shared_ptr address, connection_params params);

  void                     disconnect_unsafe(connection_map::iterator itr);
  void                     disconnect_and_clear_unsafe(connection_map::iterator itr);
  void                     disconnect_failure_unsafe(connection_map::iterator itr, int errno_err, int gai_err);

  int                 router_family() const;

  bool                try_write_with_queues(key_type id, connection_info* info);
  [[nodiscard]] int   try_write(key_type id, connection_info* info);
  [[nodiscard]] int   do_write(key_type id, connection_info* info);

  void                clear_timeout(connection_info* info);
  void                queue_timeout(key_type id, connection_info* info);
  void                queue_write(key_type id, connection_info* info);

  void                receive_timeout();

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

  // TODO: Change Thread/Scheduler callbacks to use deque, and create callback handles.

  system::Thread*       m_thread;
  random_engine         m_random_engine;

  unknown_func          m_unknown_callback;

  connection_map        m_connections;
  write_queue_type      m_write_queue;
  timeout_queue_type    m_timeout_queue;

  system::SchedulerEntry m_task_timeout;

  system::callback_id   m_resolver_callback_id;
};

inline bool UdpRouter::has_connection_id(key_type id) const  { return m_connections.find(id) != m_connections.end(); }
inline void UdpRouter::set_unknown_callback(unknown_func fn) { m_unknown_callback = std::move(fn); }
inline int  UdpRouter::router_family() const                 { return socket_address()->sa_family; }

// TODO: Move to dht_common.h.
class dht_error : public network_error {
public:
  dht_error(int code, const char* message) : m_message(message), m_code(code) {}

  int                 code() const noexcept          { return m_code; }
  const char*         what() const noexcept override { return m_message; }

private:
  const char*         m_message;
  int                 m_code;
};

} // namespace torrent::dht

#endif
