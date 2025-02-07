#include "config.h"

#include "torrent/tracker/tracker_manager.h"

#include "torrent/exceptions.h"
#include "torrent/download_info.h"
#include "torrent/tracker.h"
#include "torrent/tracker_controller.h"
#include "torrent/utils/log.h"

// TODO: Remove old logging categories.

#define LT_LOG_TRACKER_EVENTS(log_fmt, ...)                             \
  lt_log_print(LOG_TRACKER_EVENTS, "tracker_manager", log_fmt, __VA_ARGS__);

namespace torrent {

// While we pass DownloadInfo to add() method, we don't own it so we must copy any data we need.
TrackerWrapper
TrackerManager::add_tracker(DownloadInfo* download_info, Tracker* tracker_worker) {
  std::lock_guard<std::mutex> guard(m_lock);

  auto wrapper = TrackerWrapper(download_info->hash(), std::shared_ptr<Tracker>(tracker_worker));
  auto result  = m_trackers.insert(wrapper);

  if (!result.second)
    throw internal_error("TrackerManager::add(...) tracker already exists.");

  LT_LOG_TRACKER_EVENTS("added tracker: info_hash:%s url:%s", download_info->hash().c_str(), tracker_worker->url().c_str());

  return wrapper;
}

void
TrackerManager::remove_tracker(TrackerWrapper tracker) {
  std::lock_guard<std::mutex> guard(m_lock);

  // We assume there are other references to the tracker, so gracefully close it.

  m_trackers.erase(tracker);

  LT_LOG_TRACKER_EVENTS("removed tracker: info_hash:%s url:%s", tracker.info_hash().c_str(), tracker.get()->url().c_str());
}

TrackerControllerWrapper
TrackerManager::add_controller(DownloadInfo* download_info, TrackerController* controller) {
  std::lock_guard<std::mutex> guard(m_lock);

  auto wrapper = TrackerControllerWrapper(download_info->hash(), std::shared_ptr<TrackerController>(controller));
  auto result  = m_controllers.insert(wrapper);

  if (!result.second)
    throw internal_error("TrackerManager::add_controller(...) controller already exists.");

  LT_LOG_TRACKER_EVENTS("added controller: info_hash:%s", download_info->hash().c_str());

  return wrapper;
}

} // namespace torrent
