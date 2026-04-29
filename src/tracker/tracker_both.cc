#include "config.h"

#include "tracker/tracker_both.h"

#include "tracker/tracker_http.h"
#include "tracker/tracker_udp.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_hash(LOG_TRACKER_REQUESTS, info().info_hash, "tracker_both", "%p : " log_fmt, static_cast<TrackerWorker*>(this), __VA_ARGS__);

namespace torrent {

TrackerBoth::TrackerBoth(const TrackerInfo& info, int flags, bool is_udp)
  : TrackerWorker(info, flags) {

  // Need special handling of TrackerState.


  // TODO: Add flags to make the trackers only do IPv4 or IPv6.

  if (is_udp) {
    m_inet_tracker  = std::make_unique<TrackerUdp>(info, flags);
    m_inet6_tracker = std::make_unique<TrackerUdp>(info, flags);
  } else {
    m_inet_tracker  = std::make_unique<TrackerHttp>(info, flags);
    m_inet6_tracker = std::make_unique<TrackerHttp>(info, flags);
  }
}

} // namespace torrent
