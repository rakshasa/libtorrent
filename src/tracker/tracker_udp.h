#ifndef LIBTORRENT_TRACKER_TRACKER_UDP_H
#define LIBTORRENT_TRACKER_TRACKER_UDP_H

#include <array>
#include <memory>

#include "net/protocol_buffer.h"
#include "torrent/net/types.h"
#include "torrent/utils/scheduler.h"
#include "tracker/tracker_worker.h"

namespace torrent {

namespace tracker {
class UdpRouter;
} // namespace tracker


class TrackerUdp : public TrackerWorker {
public:
  using buffer_type = ProtocolBuffer<512>;

  static constexpr uint64_t magic_connection_id = 0x0000041727101980ll;

  // static constexpr uint32_t udp_timeout = 30;
  // static constexpr uint32_t udp_tries = 2;

  TrackerUdp(const TrackerInfo& info, int flags = 0);
  ~TrackerUdp() override;

  bool                is_busy() const override;

  void                send_event(tracker::TrackerState::event_enum new_state) override;
  void                send_scrape() override;

  void                close() override;

  tracker_enum        type() const override;

private:
  void                close_directly();
  void                reset_family_with_error(int family, const std::string& msg);

  uint64_t&           connection_id_for_family(int family);
  uint32_t&           transaction_id_for_family(int family);
  tracker::UdpRouter* router_for_family(int family);

  void                connect_family(int family);

  int                 process_header(int family, uint32_t action, buffer_type& buffer);

  void                prepare_connect(int family, uint32_t id, buffer_type& buffer);
  bool                process_connect(int family, uint32_t id, buffer_type& buffer);

  void                prepare_announce(int family, uint32_t id, buffer_type& buffer);
  bool                process_announce(int family, uint32_t id, buffer_type& buffer);

  void                process_error(int family, uint32_t id, buffer_type& buffer);

  void                handle_setup_error(const std::string& msg);
  bool                handle_parse_error(int family, uint32_t id, const std::string& msg);
  void                handle_udp_error(int family, uint32_t id, int errno_err, int gai_err);

  // TODO: Create a helper struct for connections (retries, failures, etc) and use that for each
  // inet/inet6 for both http and udp trackers.

  std::string         m_hostname;
  uint16_t            m_port{};

  uint64_t            m_inet_connection_id{};
  uint32_t            m_inet_transaction_id{};

  uint64_t            m_inet6_connection_id{};
  uint32_t            m_inet6_transaction_id{};

  int                 m_send_state{};
};

} // namespace torrent

#endif
