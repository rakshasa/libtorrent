#include "config.h"

#include "tracker/tracker_both.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_hash(LOG_TRACKER_REQUESTS, info().info_hash, "tracker_both", "%p : " log_fmt, static_cast<TrackerWorker*>(this), __VA_ARGS__);

namespace torrent {

TrackerBoth::TrackerBoth(const TrackerInfo& info, int flags, bool is_udp)
  : TrackerWorker(info, flags) {



  // TODO: Replace TrackerUdp::parse_udp_url and the above with a proper url parser / splitter.



}

} // namespace torrent
