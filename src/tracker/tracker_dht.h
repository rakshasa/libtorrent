#ifndef LIBTORRENT_TRACKER_TRACKER_DHT_H
#define LIBTORRENT_TRACKER_TRACKER_DHT_H

#include "net/address_list.h"
#include "torrent/object.h"
#include "tracker/tracker_worker.h"

namespace torrent {

class TrackerDht : public TrackerWorker {
public:
  TrackerDht(TrackerList* parent, const std::string& url, int flags);
  ~TrackerDht();

  typedef enum {
    state_idle,
    state_searching,
    state_announcing,
  } state_type;

  static const char* states[];

  static bool         is_allowed();

  bool                is_busy() const override;
  bool                is_usable() const override;

  void                send_event(TrackerState::event_enum new_state) override;
  void                send_scrape() override;

  void                close() override;
  void                disown() override;

  tracker_enum        type() const override;

  void                get_status(char* buffer, int length);

  state_type          get_dht_state() const            { return m_dht_state; }
  void                set_dht_state(state_type state)  { m_dht_state = state; }

  bool                has_peers() const                { return !m_peers.empty(); }

  void                receive_peers(raw_list peers);
  void                receive_success();
  void                receive_failed(const char* msg);
  void                receive_progress(int replied, int contacted);

private:
  AddressList  m_peers;
  state_type   m_dht_state;

  int          m_replied;
  int          m_contacted;
};

}

#endif
