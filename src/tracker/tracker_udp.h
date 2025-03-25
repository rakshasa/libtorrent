#ifndef LIBTORRENT_TRACKER_TRACKER_UDP_H
#define LIBTORRENT_TRACKER_TRACKER_UDP_H

#include <array>
#include <memory>

#include "net/protocol_buffer.h"
#include "net/socket_datagram.h"
#include "rak/priority_queue_default.h"
#include "rak/socket_address.h"
#include "torrent/net/types.h"
#include "tracker/tracker_worker.h"

namespace torrent {

class TrackerUdp : public SocketDatagram, public TrackerWorker {
public:
  typedef std::array<char, 1024> hostname_type;

  typedef ProtocolBuffer<512> ReadBuffer;
  typedef ProtocolBuffer<512> WriteBuffer;

  static const uint64_t magic_connection_id = 0x0000041727101980ll;

  static const uint32_t udp_timeout = 30;
  static const uint32_t udp_tries = 2;

  TrackerUdp(const TrackerInfo& info, int flags = 0);
  ~TrackerUdp();

  const char*         type_name() const override { return "tracker_udp"; }

  bool                is_busy() const override;

  void                send_event(tracker::TrackerState::event_enum new_state) override;
  void                send_scrape() override;

  void                close() override;
  void                disown() override;

  tracker_enum        type() const override;

  void                event_read() override;
  void                event_write() override;
  void                event_error() override;

private:
  void                close_directly();

  void                receive_failed(const std::string& msg);
  void                receive_resolved(c_sin_shared_ptr& sin, c_sin6_shared_ptr& sin6, int err);
  void                receive_timeout();

  void                start_announce();

  void                prepare_connect_input();
  void                prepare_announce_input();

  bool                process_connect_output();
  bool                process_announce_output();
  bool                process_error_output();

  bool                parse_udp_url(const std::string& url, hostname_type& hostname, int& port) const;

  bool                m_resolver_requesting{false};
  bool                m_sending_announce{false};

  sockaddr*           m_current_address{nullptr};
  sin_unique_ptr      m_inet_address;
  sin6_unique_ptr     m_inet6_address;

  int                 m_port{0};

  int                 m_send_state{0};

  uint32_t            m_action;
  uint64_t            m_connectionId;
  uint32_t            m_transactionId;

  std::unique_ptr<ReadBuffer>  m_readBuffer;
  std::unique_ptr<WriteBuffer> m_writeBuffer;

  uint32_t            m_tries;

  rak::priority_item  m_taskTimeout;
};

}

#endif
