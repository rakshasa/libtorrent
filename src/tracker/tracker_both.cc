#include "config.h"

#include "tracker/tracker_both.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_hash(LOG_TRACKER_REQUESTS, info().info_hash, "tracker_both", "%p : " log_fmt, static_cast<TrackerWorker*>(this), __VA_ARGS__);

namespace torrent {

TrackerBoth::TrackerBoth(const TrackerInfo& info, int flags)
  : TrackerWorker(info, flags) {

  // TODO: Check the tracker URL to determine if we need to create one or both trackers, and what kind.

  // m_inet_tracker = std::make_unique<TrackerHttp>(info, flags);
  // m_inet6_tracker = std::make_unique<TrackerHttp>(info, flags);

  // if (std::strncmp("http://", url.c_str(), 7) == 0 ||
  //     std::strncmp("https://", url.c_str(), 8) == 0) {
  //   worker = new TrackerHttp(tracker_info, flags);

  // } else if (std::strncmp("udp://", url.c_str(), 6) == 0) {
  //   worker = new TrackerUdp(tracker_info, flags);

  // } else if (std::strncmp("dht://", url.c_str(), 6) == 0 && TrackerDht::is_allowed()) {
  //   worker = new TrackerDht(tracker_info, flags);

  // } else {
  //   LT_LOG("could find matching tracker protocol : url:%s", url.c_str());

  //   if (extra_tracker)
  //     throw torrent::input_error("could find matching tracker protocol (url:" + url + ")");

  //   return;
  // }



  // TODO: Replace TrackerUdp::parse_udp_url and the above with a proper url parser / splitter.







}
