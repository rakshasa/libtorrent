#ifndef LIBTORRENT_TRACKER_H
#define LIBTORRENT_TRACKER_H

#include <array>
#include <atomic>
#include <cinttypes>
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
  bool                is_in_use() const         { return is_enabled() && m_state.load().success_counter() != 0; }

  bool                can_scrape() const        { return (m_flags & flag_can_scrape); }

  virtual bool        is_busy() const = 0;
  bool                is_busy_not_scrape() const { return m_state.load().latest_event() != EVENT_SCRAPE && is_busy(); }
  virtual bool        is_usable() const          { return is_enabled(); }

  bool                can_request_state() const;

  void                enable();
  void                disable();

  TrackerList*        parent()                              { return m_parent; }

  uint32_t            group() const                         { return m_group; }
  virtual Type        type() const = 0;

  const std::string&  url() const                           { return m_url; }
  void                set_url(const std::string& url)       { m_url = url; }

  std::string         tracker_id() const                    { return std::string(m_tracker_id.load().data()); }

  TrackerState        state() const                         { return m_state.load(); }

  virtual void        get_status(char* buffer, [[maybe_unused]] int length)  { buffer[0] = 0; }

  static std::string  scrape_url_from(std::string url);

protected:
  Tracker(TrackerList* parent, const std::string& url, int flags = 0);
  Tracker(const Tracker& t);
  void operator = (const Tracker& t);

  virtual void        send_state(int state) = 0;
  virtual void        send_scrape();
  virtual void        close() = 0;
  virtual void        disown() = 0;

  // Safeguard to catch bugs that lead to hammering of trackers.
  void                inc_request_counter();

  void                clear_stats();

  void                set_group(uint32_t v) { m_group = v; }

  // Only the tracker thread should call these.
  void                clear_intervals();
  void                set_latest_event(int v);
  void                update_tracker_id(const std::string& id);

  int                 m_flags;

  TrackerList*        m_parent;
  uint32_t            m_group{0};

  std::string                      m_url;
  std::atomic<std::array<char,64>> m_tracker_id{{}};

  // Timing of the last request, and a counter for how many requests
  // there's been in the recent past.
  uint32_t            m_request_time_last;
  uint32_t            m_request_counter{0};

  std::atomic<TrackerState> m_state;
};

inline bool
Tracker::can_request_state() const {
  return !(is_busy() && state().latest_event() != EVENT_SCRAPE) && is_usable();
}

inline void
Tracker::clear_intervals() {
  auto state = m_state.load();
  state.m_normal_interval = 0;
  state.m_min_interval = 0;
  m_state.store(state);
}

inline void
Tracker::set_latest_event(int v) {
  auto state = m_state.load();
  state.m_latest_event = v;
  m_state.store(state);
}

}

#endif
