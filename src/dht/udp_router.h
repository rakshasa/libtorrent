#ifndef LIBTORRENT_DHT_UDP_ROUTER_H
#define LIBTORRENT_DHT_UDP_ROUTER_H

#include <deque>
#include <functional>
#include <list>
#include <unordered_map>

#include "dht/dht_transaction.h"
#include "net/protocol_buffer.h"
#include "net/socket_datagram.h"
#include "torrent/system/scheduler.h"

namespace torrent::dht {

class UdpRouter : public SocketDatagram {
public:
  using key_type         = DhtTransaction::key_type;
  using buffer_type      = ProtocolBuffer<2048>;

  using prepare_func     = std::function<void(key_type, buffer_type&)>;
  using process_func     = std::function<void(key_type, const HashString&, const sockaddr*, const DhtMessage&)>;
  using error_reply_func = std::function<void(key_type, const sockaddr*, const DhtMessage&)>;
  using failure_func     = std::function<void(key_type, int, int)>;
  using packet_sent_func = std::function<void(key_type)>;
  using query_func       = std::function<void(const HashString&, const sockaddr*, const DhtMessage&)>;
  using error_func       = std::function<void(const DhtMessage&, const sockaddr*, int, const char*)>;
  using inactive_func    = std::function<void(const HashString&, const sockaddr*)>;
  using write_func       = std::function<void()>;

  struct connection_params {
    prepare_func        prepare;
    process_func        process;
    error_reply_func    error_reply;
    failure_func        failure;
    packet_sent_func    packet_sent;
  };

  UdpRouter(const HashString& id, query_func query, error_func error, inactive_func inactive,
            write_func read_done, write_func write);
  ~UdpRouter() override;

  const char*         type_name() const override { return "dht_udp_router"; }

  void                open(int family, int port);
  void                close();

  void                connect(key_type id, const sockaddr* address, connection_params params);
  void                send(key_type id);
  void                disconnect(key_type id);
  void                start_write();
  void                stop_write();

private:
  UdpRouter(const UdpRouter&) = delete;
  UdpRouter& operator=(const UdpRouter&) = delete;

  static constexpr unsigned int max_read_datagrams = 64;
  static constexpr int dht_error_protocol = 203;
  static constexpr int dht_error_bad_method = 204;

  struct connection_info;

  using connection_map     = std::unordered_map<key_type, connection_info>;
  using write_queue_type   = std::deque<std::pair<key_type, connection_info*>>;
  using timeout_queue_type = std::list<std::tuple<key_type, std::chrono::seconds, connection_info*>>;

  // TODO: Add itr to self in connection_map.

  struct connection_info {
    c_sa_shared_ptr      address;

    prepare_func         prepare{};
    process_func         process{};
    error_reply_func     error_reply{};
    failure_func         failure{};

    // Do not change the connection in this callback.
    packet_sent_func     packet_sent{};

    unsigned int         retry_count{};

    connection_info**    queue_ptr{};
    connection_info**    timeout_ptr{};
  };

  void                     disconnect_unsafe(connection_map::iterator itr);
  void                     disconnect_failure_unsafe(connection_map::iterator itr, int errno_err, int gai_err);

  int                 router_family() const;

  bool                try_write_with_queues(key_type id, connection_info* info);
  [[nodiscard]] int   try_write(key_type id, connection_info* info);
  [[nodiscard]] int   do_write(key_type id, connection_info* info);

  void                clear_timeout(connection_info* info);
  void                queue_timeout(key_type id, connection_info* info);
  void                queue_write(key_type id, connection_info* info);
  void                process_datagram(buffer_type& buffer, const sockaddr* sa);

  void                receive_timeout();

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

  // TODO: Change Thread/Scheduler callbacks to use deque, and create callback handles.

  system::Thread*       m_thread{};
  const HashString&     m_id;
  query_func            m_query;
  error_func            m_error;
  inactive_func         m_inactive;
  write_func            m_read_done;
  write_func            m_write;
  bool                  m_external_write{};

  connection_map        m_connections;
  write_queue_type      m_write_queue;
  timeout_queue_type    m_timeout_queue;

  system::SchedulerEntry m_task_timeout;
};

inline int UdpRouter::router_family() const { return socket_address()->sa_family; }

class DhtError : public network_error {
public:
  DhtError(int code, const char* message) : m_message(message), m_code(code) {}

  int         code() const noexcept { return m_code; }
  const char* what() const noexcept override { return m_message; }

private:
  const char* m_message;
  int m_code;
};

} // namespace torrent::dht

#endif
