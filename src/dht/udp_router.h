#ifndef LIBTORRENT_DHT_UDP_ROUTER_H
#define LIBTORRENT_DHT_UDP_ROUTER_H

#include <functional>

#include "net/socket_datagram.h"

namespace torrent::dht {

class UdpRouter : public SocketDatagram {
public:
  using process_func = std::function<void(const char*, int, const sockaddr*)>;
  using write_func = std::function<void()>;

  UdpRouter(process_func process, write_func read_done, write_func write);
  ~UdpRouter() override;

  const char* type_name() const override { return "dht"; }

  void open(int family, int port);
  void close();
  void start_write();
  void stop_write();

private:
  static constexpr unsigned int max_read_datagrams = 64;

  void event_read() override;
  void event_write() override;
  void event_error() override;

  process_func m_process;
  write_func m_read_done;
  write_func m_write;
};

} // namespace torrent::dht

#endif
