#ifndef LIBTORRENT_TRACKER_H
#define LIBTORRENT_TRACKER_H

#include <torrent/common.h>

namespace torrent {

class TrackerHttp;

class Tracker {
public:
  typedef std::pair<int, TrackerHttp*> value_type;

  Tracker()             : m_tracker(value_type(0, NULL)) {}
  Tracker(value_type v) : m_tracker(v) {}
  
  uint32_t            get_group()                        { return m_tracker.first; }
  const std::string&  get_url();

  // The "tracker id" string returned by the tracker.
  const std::string&  get_tracker_id();

private:
  value_type          m_tracker;
};

}

#endif
