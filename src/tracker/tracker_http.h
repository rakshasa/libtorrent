#ifndef LIBTORRENT_TRACKER_TRACKER_HTTP_H
#define LIBTORRENT_TRACKER_TRACKER_HTTP_H

#include <iosfwd>
#include <memory>

#include "torrent/object.h"
#include "torrent/tracker/tracker_state.h"
#include "tracker/tracker_worker.h"

namespace torrent {

class Http;

class TrackerHttp : public TrackerWorker {
public:
  static const uint32_t http_timeout = 60;

  TrackerHttp(const TrackerInfo& info, int flags = 0);

  bool                is_busy() const override;

  void                send_event(tracker::TrackerState::event_enum new_state) override;
  void                send_scrape() override;
  void                close() override;
  void                disown() override;

  tracker_enum        type() const override;

private:
  void                close_directly();

  void                request_prefix(std::stringstream* stream, const std::string& url);

  void                receive_done();
  void                receive_signal_failed(const std::string& msg);
  void                receive_failed(const std::string& msg);

  void                process_failure(const Object& object);
  void                process_success(const Object& object);
  void                process_scrape(const Object& object);

  void                update_tracker_id(const std::string& id);

  std::unique_ptr<Http>              m_get;
  std::unique_ptr<std::stringstream> m_data;

  bool                m_drop_deliminator;
  std::string         m_current_tracker_id;
};

}

#endif
