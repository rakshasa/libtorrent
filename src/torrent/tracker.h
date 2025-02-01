#ifndef LIBTORRENT_TRACKER_H
#define LIBTORRENT_TRACKER_H

#include <array>
#include <atomic>
#include <cinttypes>
#include <mutex>
#include <string>
#include <torrent/common.h>
#include <torrent/tracker/tracker_state.h>

namespace torrent {

class AddressList;
class TrackerList;

class LIBTORRENT_EXPORT Tracker {
public:
  friend class TrackerList;

  typedef enum {
    TRACKER_NONE,
    TRACKER_HTTP,
    TRACKER_UDP,
    TRACKER_DHT,
  } Type;

  enum tracker_event {
    EVENT_NONE,
    EVENT_COMPLETED,
    EVENT_STARTED,
    EVENT_STOPPED,
    EVENT_SCRAPE
  };

  static const int flag_enabled       = 0x1;
  static const int flag_extra_tracker = 0x2;
  static const int flag_can_scrape    = 0x4;

  static const int max_flag_size   = 0x10;
  static const int mask_base_flags = 0x10 - 1;

  // TODO: Move to state.
  static constexpr int default_min_interval = 600;
  static constexpr int min_min_interval     = 300;
  static constexpr int max_min_interval     = 4 * 3600;

  static constexpr int default_normal_interval = 1800;
  static constexpr int min_normal_interval     = 600;
  static constexpr int max_normal_interval     = 8 * 3600;

  virtual ~Tracker() {}

  int                 flags() const { return m_flags; }

  bool                is_enabled() const        { return (m_flags & flag_enabled); }
  bool                is_extra_tracker() const  { return (m_flags & flag_extra_tracker); }
  bool                is_in_use() const         { return is_enabled() && state().success_counter() != 0; }

  bool                can_scrape() const        { return (m_flags & flag_can_scrape); }

  virtual bool        is_busy() const = 0;
  bool                is_busy_not_scrape() const { return state().latest_event() != EVENT_SCRAPE && is_busy(); }
  virtual bool        is_usable() const          { return is_enabled(); }

  bool                can_request_state() const;

  void                enable();
  void                disable();

  TrackerList*        parent()                              { return m_parent; }

  uint32_t            group() const                         { return m_group; }
  virtual Type        type() const = 0;

  const std::string&  url() const                           { return m_url; }
  void                set_url(const std::string& url)       { m_url = url; }

  std::string         tracker_id() const;
  TrackerState        state() const;;

  virtual void        get_status(char* buffer, [[maybe_unused]] int length)  { buffer[0] = 0; }

  static std::string  scrape_url_from(std::string url);

protected:
  Tracker(TrackerList* parent, const std::string& url, int flags = 0);
  Tracker(const Tracker& t);
  void operator = (const Tracker& t);

  // TODO: Rename to send_event.
  virtual void        send_state(int state) = 0;
  virtual void        send_scrape();
  virtual void        close() = 0;
  virtual void        disown() = 0;

  // Safeguard to catch bugs that lead to hammering of trackers.
  void                inc_request_counter();

  void                set_group(uint32_t v) { m_group = v; }

  // Only the tracker thread should call these.
  void                clear_intervals();
  void                clear_stats();

  void                set_latest_event(int v);
  void                set_state(const TrackerState& state);
  void                update_tracker_id(const std::string& id);

  int                 m_flags;

  TrackerList*        m_parent;
  uint32_t            m_group{0};

  std::string         m_url;

  // Timing of the last request, and a counter for how many requests
  // there's been in the recent past.
  uint32_t            m_request_time_last;
  uint32_t            m_request_counter{0};

private:
  // Only the tracker thread should change state.
  mutable std::mutex  m_state_mutex;
  TrackerState        m_state;
  std::string         m_tracker_id;
};

inline bool
Tracker::can_request_state() const {
  return !(is_busy() && state().latest_event() != EVENT_SCRAPE) && is_usable();
}

inline std::string
Tracker::tracker_id() const {
  std::lock_guard<std::mutex> lock(m_state_mutex);
  return m_tracker_id;
}

inline TrackerState
Tracker::state() const {
  std::lock_guard<std::mutex> lock(m_state_mutex);
  return m_state;
}

// TODO: Use lambda function to update within the lock.
inline void
Tracker::set_state(const TrackerState& state) {
  std::lock_guard<std::mutex> lock(m_state_mutex);
  m_state = state;
}

}

#endif
