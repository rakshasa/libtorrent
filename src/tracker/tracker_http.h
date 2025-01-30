#ifndef LIBTORRENT_TRACKER_TRACKER_HTTP_H
#define LIBTORRENT_TRACKER_TRACKER_HTTP_H

#include <iosfwd>

#include "torrent/object.h"
#include "torrent/tracker.h"

namespace torrent {

class Http;

class TrackerHttp : public Tracker {
public:
  TrackerHttp(TrackerList* parent, const std::string& url, int flags);
  ~TrackerHttp();

  virtual bool        is_busy() const;

  virtual void        send_state(int new_state);
  virtual void        send_scrape();
  virtual void        close();
  virtual void        disown();

  virtual Type        type() const;

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
