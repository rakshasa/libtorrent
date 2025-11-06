#ifndef LIBTORRENT_TRACKER_CONTROLLER_H
#define LIBTORRENT_TRACKER_CONTROLLER_H

#include <functional>
#include <string>

#include "torrent/tracker/tracker_state.h"
#include "torrent/utils/scheduler.h"

// TODO: Remove all unused functions and slots, move to src/tracker. Then add a
// TrackerControllerWrapper that download and api uses.

namespace torrent {

class TrackerController {
public:
  using address_list = AddressList;

  using slot_void         = std::function<void(void)>;
  using slot_string       = std::function<void(const std::string&)>;
  using slot_address_list = std::function<uint32_t(AddressList*)>;
  using slot_tracker      = std::function<void(const tracker::Tracker&)>;

  static constexpr int flag_send_update      = 0x1;
  static constexpr int flag_send_completed   = 0x2;
  static constexpr int flag_send_start       = 0x4;
  static constexpr int flag_send_stop        = 0x8;

  static constexpr int flag_active           = 0x10;
  static constexpr int flag_requesting       = 0x20;
  static constexpr int flag_failure_mode     = 0x40;
  static constexpr int flag_promiscuous_mode = 0x80;

  static constexpr int mask_send = flag_send_update | flag_send_start | flag_send_stop | flag_send_completed;

  static constexpr int enable_dont_reset_stats = 0x1;

  TrackerController(TrackerList* trackers);
  ~TrackerController();

  int                 flags() const               { return m_flags; }

  bool                is_active() const           { return m_flags & flag_active; }
  bool                is_requesting() const       { return m_flags & flag_requesting; }
  bool                is_failure_mode() const     { return m_flags & flag_failure_mode; }
  bool                is_promiscuous_mode() const { return m_flags & flag_promiscuous_mode; }
  bool                is_timeout_queued() const;
  bool                is_scrape_queued() const;

  TrackerList*        tracker_list()        { return m_tracker_list; }
  TrackerList*        tracker_list() const  { return m_tracker_list; }

  int64_t             next_timeout() const;
  int64_t             next_scrape() const;
  uint32_t            seconds_to_next_timeout() const;
  uint32_t            seconds_to_next_scrape() const;

  void                manual_request(bool request_now);
  void                scrape_request(uint32_t seconds_to_request);

  void                send_start_event();
  void                send_stop_event();
  void                send_completed_event();
  void                send_update_event();

  void                close();

  void                enable(int enable_flags = 0);
  void                disable();

  void                start_requesting();
  void                stop_requesting();

  uint32_t            receive_success(const tracker::Tracker& tracker, address_list* l);
  void                receive_failure(const tracker::Tracker& tracker, const std::string& msg);
  void                receive_scrape(const tracker::Tracker& tracker) const;
  uint32_t            receive_new_peers(address_list* l);

  void                receive_tracker_enabled(const tracker::Tracker& tb);
  void                receive_tracker_disabled(const tracker::Tracker& tb);

  slot_void&          slot_timeout()          { return m_slot_timeout; }
  slot_address_list&  slot_success()          { return m_slot_success; }
  slot_string&        slot_failure()          { return m_slot_failure; }

  slot_tracker&       slot_tracker_enabled()  { return m_slot_tracker_enabled; }
  slot_tracker&       slot_tracker_disabled() { return m_slot_tracker_disabled; }

private:
  TrackerController(const TrackerController&) = delete;
  TrackerController& operator=(const TrackerController&) = delete;

  void                do_timeout();
  void                do_scrape();

  void                update_timeout(uint32_t seconds_to_next);

  inline tracker::TrackerState::event_enum current_send_event() const;

  int                 m_flags{0};
  TrackerList*        m_tracker_list;

  slot_void           m_slot_timeout;
  slot_address_list   m_slot_success;
  slot_string         m_slot_failure;

  slot_tracker        m_slot_tracker_enabled;
  slot_tracker        m_slot_tracker_disabled;

  utils::SchedulerEntry m_task_timeout;
  utils::SchedulerEntry m_task_scrape;
};

uint32_t tracker_next_timeout(const tracker::Tracker& tracker, int controller_flags);
uint32_t tracker_next_timeout_update(const tracker::Tracker& tracker);
uint32_t tracker_next_timeout_promiscuous(const tracker::Tracker& tracker);

} // namespace torrent

#endif
