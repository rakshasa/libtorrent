#include "config.h"

#include "torrent/tracker/tracker_manager.h"

#include "torrent/exceptions.h"
#include "torrent/download_info.h"
#include "torrent/tracker.h"
#include "torrent/tracker_controller.h"
#include "torrent/utils/log.h"

#define LT_LOG_TRACKER_EVENTS(log_fmt, ...)                             \
  lt_log_print_subsystem(LOG_TRACKER_EVENTS, "tracker_manager", log_fmt, __VA_ARGS__);

namespace torrent::tracker {

TrackerControllerWrapper
TrackerManager::add_controller(DownloadInfo* download_info, TrackerController* controller) {
  if (download_info->hash() == HashString::new_zero())
    throw internal_error("TrackerManager::add(...) invalid info_hash.");

  std::lock_guard<std::mutex> guard(m_lock);

  auto wrapper = TrackerControllerWrapper(download_info->hash(), std::shared_ptr<TrackerController>(controller));
  auto result  = m_controllers.insert(wrapper);

  if (!result.second)
    throw internal_error("TrackerManager::add_controller(...) controller already exists.");

  LT_LOG_TRACKER_EVENTS("added controller: info_hash:%s", hash_string_to_hex_str(download_info->hash()).c_str());

  return wrapper;
}

void
TrackerManager::remove_controller(TrackerControllerWrapper controller) {
  std::lock_guard<std::mutex> guard(m_lock);

  // We assume there are other references to the controller, so gracefully close it.
  if (m_controllers.erase(controller) != 1)
    throw internal_error("TrackerManager::remove_controller(...) controller not found or has multiple references.");

  LT_LOG_TRACKER_EVENTS("removed controller: info_hash:%s", hash_string_to_hex_str(controller.info_hash()).c_str());
}

} // namespace torrent
