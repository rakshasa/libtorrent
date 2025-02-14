#ifndef LIBTORRENT_TRACKER_TRACKER_HTTP_H
#define LIBTORRENT_TRACKER_TRACKER_HTTP_H

#include <iosfwd>

#include "torrent/object.h"
#include "torrent/tracker/tracker_state.h"
#include "tracker/tracker_worker.h"

namespace torrent {

class Http;

class TrackerHttp : public TrackerWorker {
public:
  TrackerHttp(TrackerList* parent, const std::string& url, int flags);
  ~TrackerHttp();

  bool                is_busy() const override;

  void                send_event(TrackerState::event_enum new_state) override;
  void                send_scrape() override;
  void                close() override;
  void                disown() override;

  tracker_enum        type() const override;

private:
  void                close_directly();

  void                request_prefix(std::stringstream* stream, const std::string& url);

  void                receive_done();
  void                receive_signal_failed(std::string msg);
  void                receive_failed(std::string msg);

  void                process_failure(const Object& object);
  void                process_success(const Object& object);
  void                process_scrape(const Object& object);

  Http*               m_get;
  std::stringstream*  m_data;

  bool                m_dropDeliminator;
};

}

#endif
