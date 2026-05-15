#ifndef LIBTORRENT_TRACKER_TRACKER_HTTP_H
#define LIBTORRENT_TRACKER_TRACKER_HTTP_H

#include <iosfwd>
#include <memory>
#include <tuple>

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
  ~TrackerHttp() noexcept(false) override;

  tracker_enum        type() const override;

  void                send_event(tracker::TrackerParams params, tracker::TrackerState::event_enum new_state) override;
  void                send_scrape(tracker::TrackerParams params) override;

  void                close() override;

private:
  void                close_directly();

  void                update_requesting_state();

  void                send_event_unsafe(tracker::TrackerState::event_enum state);
  void                send_scrape_unsafe();
  bool                send_next_family(bool scrape = false);

  void                delayed_send_scrape();

  std::stringstream   request_prefix(const std::string& url);
  std::string         request_announce_url(tracker::TrackerState::event_enum state, int family);
  std::tuple<int,int> request_families();

  void                update_tracker_id(const std::string& id);

  void                receive_done();
  void                receive_signal_failed(const std::string& msg);
  void                receive_failed(const std::string& msg);

  void                process_failure(const Object& object);
  void                process_success(const Object& object);
  void                process_scrape(const Object& object);

  tracker::TrackerParams m_params;

  net::HttpGet                       m_get;
  std::shared_ptr<std::stringstream> m_data;

  int                 m_hostname_family{};
  int                 m_current_family{};
  int                 m_next_family{};

  bool                m_last_success{};
  std::string         m_last_error_message;
  bool                m_requested_scrape{};

  utils::SchedulerEntry m_delay_scrape;
};

} // namespace torrent

#endif
