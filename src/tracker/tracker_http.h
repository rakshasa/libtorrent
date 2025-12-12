#ifndef LIBTORRENT_TRACKER_TRACKER_HTTP_H
#define LIBTORRENT_TRACKER_TRACKER_HTTP_H

#include <iosfwd>
#include <memory>

#include "torrent/object.h"
#include "tracker/tracker_worker.h"
#include "torrent/net/http_get.h"
#include "torrent/tracker/tracker_state.h"
#include "torrent/utils/scheduler.h"

namespace torrent {

class Http;

class TrackerHttp : public TrackerWorker {
public:
  static constexpr uint32_t http_timeout = 60;

  TrackerHttp(const TrackerInfo& info, int flags = 0);
  ~TrackerHttp() override;

  tracker_enum        type() const override;

  bool                is_busy() const override;

  void                send_event(tracker::TrackerState::event_enum new_state) override;
  void                send_scrape() override;
  void                close() override;

private:
  void                close_directly();

  void                request_prefix(std::stringstream* stream, const std::string& url);
  std::string         request_announce_url(tracker::TrackerState::event_enum state, TrackerParameters params, int family);

  void                delayed_send_scrape();

  void                receive_done();
  void                receive_signal_failed(const std::string& msg);
  void                receive_failed(const std::string& msg);

  void                process_failure(const Object& object);
  void                process_success(const Object& object);
  void                process_scrape(const Object& object);

  void                update_tracker_id(const std::string& id);

  net::HttpGet                       m_get;
  std::shared_ptr<std::stringstream> m_data;

  bool                  m_drop_deliminator{};
  std::string           m_current_tracker_id;

  bool                  m_requested_scrape{};
  utils::SchedulerEntry m_delay_scrape;
};

} // namespace torrent

#endif
