#include "config.h"

#include "torrent/tracker/tracker_manager.h"

#include "torrent/exceptions.h"
#include "torrent/download_info.h"
#include "torrent/tracker.h"
#include "torrent/utils/log.h"

// TODO: Remove old logging categories.

#define LT_LOG_TRACKER_EVENTS(log_level, log_fmt, ...)                  \
  lt_log_print(LOG_TRACKER_EVENTS, "tracker_manager", log_fmt, __VA_ARGS__);

namespace torrent {

// While we pass DownloadInfo to add() method, we don't own it so we must copy any data we need.
TrackerWrapper TrackerManager::add(DownloadInfo* download_info, Tracker* tracker_worker) {
  std::lock_guard<std::mutex> guard(m_lock);

  auto tracker = TrackerWrapper(download_info->hash(), std::shared_ptr<Tracker>(tracker_worker));
  auto result = base_type::insert(tracker);

  if (!result.second)
    throw internal_error("TrackerManager::add(...) tracker already exists.");

  LT_LOG_TRACKER_EVENTS("added tracker: info_hash:%s url:%s", tracker.info_hash().c_str(), tracker.get()->url().c_str());

  return tracker;
}

void TrackerManager::remove(TrackerWrapper tracker) {
  std::lock_guard<std::mutex> guard(m_lock);

  // We assume there are other references to the tracker, so gracefully close it.

  base_type::erase(tracker);

  LT_LOG_TRACKER_EVENTS("removed tracker: info_hash:%s url:%s", tracker.info_hash().c_str(), tracker.get()->url().c_str());
}

} // namespace torrent
