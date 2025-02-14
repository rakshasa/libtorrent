#ifndef LIBTORRENT_TRACKER_H
#define LIBTORRENT_TRACKER_H

#include <array>
#include <atomic>
#include <cinttypes>
#include <memory>
#include <mutex>
#include <string>
#include <torrent/common.h>
#include <torrent/tracker/tracker_state.h>

namespace torrent {

class AddressList;
class TrackerList;
class TrackerWorker;

class LIBTORRENT_EXPORT Tracker {
public:
  friend class TrackerList;

  virtual ~Tracker() = default;
  Tracker(const Tracker&) = delete;

  Tracker& operator=(const Tracker&) = delete;

  bool                is_enabled() const;
  bool                is_extra_tracker() const;
  bool                is_in_use() const;

  bool                is_busy() const;
  bool                is_busy_not_scrape() const;
  bool                is_usable() const;

  bool                can_request_state() const;
  bool                can_scrape() const;

  void                enable();
  void                disable();

  // TrackerList*        parent()                              { return m_parent; }

  uint32_t             group() const                        { return m_group; }
  // virtual tracker_enum type() const = 0;

  const std::string&  url() const;

  std::string         tracker_id() const;
  TrackerState        state() const;;

  virtual void        get_status(char* buffer, [[maybe_unused]] int length)  { buffer[0] = 0; }

protected:
  // Tracker(TrackerList* parent, const std::string& url, int flags = 0);
  Tracker(TrackerList* parent, std::shared_ptr<TrackerWorker>&& worker);

  // virtual void        send_event(TrackerState::event_enum state) = 0;
  // virtual void        send_scrape();
  // virtual void        close() = 0;
  // virtual void        disown() = 0;

  TrackerWorker*      get() { return m_worker.get(); }

  // Safeguard to catch bugs that lead to hammering of trackers.
  void                inc_request_counter();

  void                set_group(uint32_t v) { m_group = v; }

  TrackerList*        m_parent;
  uint32_t            m_group{0};

  // Timing of the last request, and a counter for how many requests
  // there's been in the recent past.
  uint32_t            m_request_time_last;
  uint32_t            m_request_counter{0};

private:
  std::shared_ptr<TrackerWorker> m_worker;
};

}

#endif
