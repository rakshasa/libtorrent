#ifndef LIBTORRENT_TRACKER_TRACKER_H
#define LIBTORRENT_TRACKER_TRACKER_H

#include <functional>
#include <memory>
#include <string>
#include <torrent/common.h>
#include <torrent/tracker/tracker_state.h>

class TrackerTest;

namespace RTORRENT_EXPORT torrent {

class ThreadTracker;

namespace tracker {

class Tracker {
public:
  bool                is_valid() const { return m_worker != nullptr; }

  bool                is_enabled() const;
  bool                is_requesting() const;
  bool                is_requesting_not_scrape() const;
  bool                is_requesting_not_dht() const;
  bool                is_requesting_not_dht_scrape() const;
  bool                is_requesting_not_dht_scrape_disownable() const;
  bool                is_extra_tracker() const;
  bool                is_in_use() const;
  bool                is_usable() const;
  bool                is_scrapable() const;
  bool                is_disownable() const;

  bool                can_request_state() const;

  tracker_enum        type() const;
  const std::string&  url() const;
  uint32_t            group() const;

  std::string         tracker_id() const;

  void                enable();
  void                disable();

  TrackerState        state() const;
  std::string         status() const;

  void                lock_and_call_state(const std::function<void(const TrackerState&)>& fn) const;

  bool                operator< (const Tracker& rhs) const { return m_worker < rhs.m_worker; }
  bool                operator==(const Tracker& rhs) const { return m_worker == rhs.m_worker; }

protected:
  friend class Manager;
  friend class TrackerControllerWrapper;
  friend class torrent::ThreadTracker;
  friend class torrent::TrackerList;
  friend class ::TrackerTest;

  Tracker(std::shared_ptr<torrent::TrackerWorker>&& worker);

  static Tracker      from_weak_ptr(const std::weak_ptr<TrackerWorker>& worker) { return Tracker(worker.lock()); }

  TrackerWorker*      get_worker()                                              { return m_worker.get(); }
  auto                get_weak_ptr() const                                      { return std::weak_ptr<TrackerWorker>(m_worker); }

  void                close();

  void                clear_stats();

private:
  std::shared_ptr<torrent::TrackerWorker> m_worker;
};

} // namespace torrent::tracker

} // namespace torrent

#endif
