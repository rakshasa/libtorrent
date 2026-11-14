#ifndef LIBTORRENT_DHT_UDP_ROUTER_H
#define LIBTORRENT_DHT_UDP_ROUTER_H

#include <functional>
#include <unordered_map>

#include "dht/dht_transaction.h"
#include "net/socket_datagram.h"
#include "torrent/exceptions.h"
#include "torrent/system/common.h"

namespace torrent::dht {

class DhtError : public network_error {
public:
  DhtError(int code, const char* message) : m_message(message), m_code(code) {}

  int         code() const noexcept { return m_code; }
  const char* what() const noexcept override { return m_message; }

private:
  const char* m_message;
  int m_code;
};

class UdpRouter : public SocketDatagram {
public:
  using key_type      = DhtTransaction::key_type;
  using process_func  = std::function<void(key_type, const HashString&, const sockaddr*, const DhtMessage&)>;
  using failure_func  = std::function<void(key_type, const sockaddr*, const DhtMessage&)>;
  using query_func    = std::function<void(const HashString&, const sockaddr*, const DhtMessage&)>;
  using error_func    = std::function<void(const DhtMessage&, const sockaddr*, int, const char*)>;
  using inactive_func = std::function<void(const HashString&, const sockaddr*)>;
  using write_func    = std::function<void()>;

  struct connection_params {
    process_func process;
    failure_func failure;
  };

  UdpRouter(const HashString& id, query_func query, error_func error, inactive_func inactive,
            write_func read_done, write_func write);
  ~UdpRouter() override;

  const char* type_name() const override { return "dht"; }

  void open(int family, int port);
  void close();
  void connect(key_type key, const sockaddr* address, connection_params params);
  void disconnect(key_type key);
  void start_write();
  void stop_write();

private:
  UdpRouter(const UdpRouter&) = delete;
  UdpRouter& operator=(const UdpRouter&) = delete;

  static constexpr unsigned int max_read_datagrams = 64;
  static constexpr int dht_error_protocol = 203;
  static constexpr int dht_error_bad_method = 204;

  struct connection_info {
    c_sa_unique_ptr address;
    process_func process;
    failure_func failure;
  };

  using connection_map = std::unordered_map<key_type, connection_info>;

  void process_datagram(const char* buffer, int length, const sockaddr* sa);
  void event_read() override;
  void event_write() override;
  void event_error() override;

  const HashString& m_id;
  query_func m_query;
  error_func m_error;
  inactive_func m_inactive;
  write_func m_read_done;
  write_func m_write;
  connection_map m_connections;
  system::Thread*       m_thread{};
};

} // namespace torrent::dht

#endif // LIBTORRENT_DHT_UDP_ROUTER_H
