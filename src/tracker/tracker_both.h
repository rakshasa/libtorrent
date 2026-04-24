#ifndef LIBTORRENT_TRACKER_TRACKER_BOTH_H
#define LIBTORRENT_TRACKER_TRACKER_BOTH_H

#include <iosfwd>
#include <memory>

#include "tracker/tracker_worker.h"
#include "torrent/tracker/tracker_state.h"

namespace torrent {

class Both;

class TrackerBoth : public TrackerWorker {
public:
  TrackerBoth(const TrackerInfo& info, int flags = 0);
  ~TrackerBoth() override;

  tracker_enum        type() const override;

  bool                is_busy() const override;

  void                send_event(tracker::TrackerState::event_enum new_state) override;
  void                send_scrape() override;

  void                close() override;

private:
  void                receive_done();
  void                receive_signal_failed(const std::string& msg);
  void                receive_failed(const std::string& msg);

  void                process_failure(const Object& object);
  void                process_success(const Object& object);
  void                process_scrape(const Object& object);

  std::unique_ptr<TrackerWorker> m_inet_tracker;
  std::unique_ptr<TrackerWorker> m_inet6_tracker;
};

} // namespace torrent

#endif
