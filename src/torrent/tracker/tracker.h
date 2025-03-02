#ifndef LIBTORRENT_TRACKER_TRACKER_H
#define LIBTORRENT_TRACKER_TRACKER_H

#include <functional>
#include <memory>
#include <string>
#include <torrent/common.h>
#include <torrent/tracker/tracker_state.h>

class TrackerTest;

namespace torrent::tracker {

class LIBTORRENT_EXPORT Tracker {
public:
  bool                is_valid() const { return m_worker != nullptr; }

  bool                is_busy() const;
  bool                is_busy_not_scrape() const;
  bool                is_enabled() const;
  bool                is_extra_tracker() const;
  bool                is_in_use() const;
  bool                is_usable() const;
  bool                is_scrapable() const;

  bool                can_request_state() const;

  void                enable();
  void                disable();

  torrent::tracker_enum type() const;
  const std::string&    url() const;

  std::string         tracker_id() const;
  TrackerState        state() const;;
  std::string         status() const;

  // If the tracker group is changed, it not be updated for Tracker objects outside of TrackerList.
  uint32_t            group() const { return m_group; }

  void                lock_and_call_state(std::function<void(const TrackerState&)> f) const;

  bool                operator< (const Tracker& rhs) const { return m_worker < rhs.m_worker; }
  bool                operator==(const Tracker& rhs) const { return m_worker == rhs.m_worker; }

  static Tracker      create_empty() { return Tracker(nullptr); }

protected:
  friend class torrent::TrackerList;
  friend class ::TrackerTest;

  Tracker(std::shared_ptr<torrent::TrackerWorker>&& worker);

  TrackerWorker*      get_worker() { return m_worker.get(); }

  void                clear_stats();
  void                set_group(uint32_t v) { m_group = v; }

private:
  uint32_t                                m_group{0};
  std::shared_ptr<torrent::TrackerWorker> m_worker;
};

} // namespace torrent::tracker

#endif
