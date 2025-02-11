#ifndef LIBTORRENT_TRACKER_TRACKER_UDP_H
#define LIBTORRENT_TRACKER_TRACKER_UDP_H

#include <array>
#include <rak/socket_address.h>

#include "net/protocol_buffer.h"
#include "net/socket_datagram.h"
#include "torrent/connection_manager.h"
#include "torrent/tracker.h"

#include "globals.h"

namespace torrent {

class TrackerUdp : public SocketDatagram, public Tracker {
public:
  typedef std::array<char, 1024> hostname_type;

  typedef ProtocolBuffer<512> ReadBuffer;
  typedef ProtocolBuffer<512> WriteBuffer;

  typedef ConnectionManager::slot_resolver_result_type resolver_type;

  static const uint64_t magic_connection_id = 0x0000041727101980ll;

  TrackerUdp(TrackerList* parent, const std::string& url, int flags);
  ~TrackerUdp();

  const char*         type_name() const { return "tracker_udp"; }

  virtual bool        is_busy() const;

  virtual void        send_event(TrackerState::event_enum new_state);

  virtual void        close();
  virtual void        disown();

  virtual tracker_enum type() const;

  virtual void        event_read();
  virtual void        event_write();
  virtual void        event_error();

private:
  void                close_directly();

  void                receive_failed(const std::string& msg);
  void                receive_timeout();

  void                start_announce(const sockaddr* sa, int err);

  void                prepare_connect_input();
  void                prepare_announce_input();

  bool                process_connect_output();
  bool                process_announce_output();
  bool                process_error_output();

  bool                parse_udp_url(const std::string& url, hostname_type& hostname, int& port) const;
  resolver_type*      make_resolver_slot(const hostname_type& hostname);

  rak::socket_address m_connectAddress;
  int                 m_port{0};

  int                 m_send_state{0};

  resolver_type*      m_slot_resolver{nullptr};

  uint32_t            m_action;
  uint64_t            m_connectionId;
  uint32_t            m_transactionId;

  ReadBuffer*         m_readBuffer{nullptr};
  WriteBuffer*        m_writeBuffer{nullptr};

  uint32_t            m_tries;

  rak::priority_item  m_taskTimeout;
};

}

#endif
