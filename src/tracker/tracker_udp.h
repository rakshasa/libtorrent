#ifndef LIBTORRENT_TRACKER_TRACKER_UDP_H
#define LIBTORRENT_TRACKER_TRACKER_UDP_H

#include <array>
#include <memory>

#include "net/protocol_buffer.h"
#include "torrent/net/types.h"
#include "torrent/utils/scheduler.h"
#include "tracker/tracker_worker.h"

namespace torrent {

class TrackerUdp : public TrackerWorker {
public:
  using ReadBuffer  = ProtocolBuffer<512>;
  using WriteBuffer = ProtocolBuffer<512>;

  static constexpr uint64_t magic_connection_id = 0x0000041727101980ll;

  static constexpr uint32_t udp_timeout = 30;
  static constexpr uint32_t udp_tries = 2;

  TrackerUdp(const TrackerInfo& info, int flags = 0);
  ~TrackerUdp() override;

  bool                is_busy() const override;

  void                send_event(tracker::TrackerState::event_enum new_state) override;
  void                send_scrape() override;

  void                close() override;

  tracker_enum        type() const override;

private:
  void                close_directly();

  void                receive_failed(const std::string& msg);
  void                receive_resolved(c_sin_shared_ptr& sin, int err, c_sin6_shared_ptr& sin6, int err6);
  void                receive_timeout();

  void                start_announce();

  void                prepare_connect_input();
  void                prepare_announce_input();

  bool                process_connect_output();
  bool                process_announce_output();
  bool                process_error_output();

  // TODO: Create a helper struct for connections (retries, failures, etc) and use that for each
  // inet/inet6 for both http and udp trackers.

  bool                m_resolver_requesting{};
  bool                m_sending_announce{};

  std::string         m_hostname;
  uint16_t            m_port{};

  sa_shared_ptr       m_inet_address;
  uint64_t            m_inet_connection_id{};
  uint32_t            m_inet_transaction_id{};

  sa_shared_ptr       m_inet6_address;
  uint64_t            m_inet6_connection_id{};
  uint32_t            m_inet6_transaction_id{};

  int                 m_send_state{};

  uint32_t            m_action{};

  // TODO: Timeout should be handled by UdpRouter
  // utils::SchedulerEntry m_task_timeout;
};

} // namespace torrent

#endif
