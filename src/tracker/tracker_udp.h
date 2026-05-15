#ifndef LIBTORRENT_TRACKER_TRACKER_UDP_H
#define LIBTORRENT_TRACKER_TRACKER_UDP_H

#include "net/protocol_buffer.h"
#include "tracker/tracker_worker.h"

namespace torrent::tracker {

class UdpRouter;

class TrackerUdp : public TrackerWorker {
public:
  TrackerUdp(const TrackerInfo& info, int flags = 0);

  tracker_enum        type() const override;

  void                send_event(TrackerParams params, TrackerState::event_enum new_state) override;
  void                send_scrape(TrackerParams params) override;

  void                close() override;

private:
  static constexpr uint64_t magic_connection_id = 0x0000041727101980ll;

  using buffer_type = ProtocolBuffer<512>;

  struct family_state {
    uint32_t transaction_id{};
    uint64_t connection_id{};
    bool     packet_sent{};
  };

  void                close_directly();
  void                cleanup() override;

  void                reset_family_with_error(int family, const std::string& msg);

  void                update_requesting_state();

  UdpRouter*          router_for_family(int family);
  family_state&       state_for_family(int family);

  void                connect_family(int family);

  int                 process_header(int family, uint32_t action, buffer_type& buffer);

  void                prepare_connect(int family, uint32_t id, buffer_type& buffer);
  bool                process_connect(int family, uint32_t id, buffer_type& buffer);

  void                prepare_announce(int family, uint32_t id, buffer_type& buffer);
  bool                process_announce(int family, uint32_t id, buffer_type& buffer);
  void                process_announce_packet_sent(int family, uint32_t id);

  void                process_error(int family, uint32_t id, buffer_type& buffer);

  void                handle_setup_error(const std::string& msg);
  bool                handle_parse_error(int family, uint32_t id, const std::string& msg);
  void                handle_udp_error(int family, uint32_t id, int errno_err, int gai_err);

  std::string         m_hostname;
  uint16_t            m_port{};

  TrackerParams       m_params;
  int                 m_send_state{};
  family_state        m_inet_state{};
  family_state        m_inet6_state{};
};

} // namespace torrent::tracker

#endif
