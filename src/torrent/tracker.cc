#include "config.h"

#include "tracker/tracker_http.h"

#include "tracker.h"

namespace torrent {

const std::string&
Tracker::get_url() {
  return m_tracker.second->get_url();
}

}
