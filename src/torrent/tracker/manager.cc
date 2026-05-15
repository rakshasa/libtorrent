#include "config.h"

#include <cassert>
#include <utility>

#include "torrent/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/tracker/manager.h"
#include "torrent/tracker/tracker.h"
#include "torrent/utils/log.h"
#include "torrent/system/thread.h"
#include "torrent/utils/string_manip.h"
#include "tracker/tracker_controller.h"
#include "tracker/tracker_list.h"
#include "tracker/tracker_worker.h"

#define LT_LOG_TRACKER_EVENTS(log_fmt, ...)                             \
  lt_log_print_subsystem(LOG_TRACKER_EVENTS, "tracker::manager", log_fmt, __VA_ARGS__);

namespace torrent::tracker {

Manager::Manager() = default;

TrackerControllerWrapper
Manager::add_controller(DownloadInfo* download_info, std::shared_ptr<TrackerController> controller) {
  assert(std::this_thread::get_id() == main_thread::thread_id());

  if (download_info->hash() == HashString::new_zero())
    throw internal_error("tracker::Manager::add(...) invalid info_hash.");

  auto guard = std::scoped_lock(m_lock);

  auto wrapper = TrackerControllerWrapper(download_info->hash(), std::move(controller));
  auto result  = m_controllers.insert(wrapper);

  if (!result.second)
    throw internal_error("tracker::Manager::add_controller(...) controller already exists.");

  LT_LOG_TRACKER_EVENTS("added controller: info_hash:%s", utils::transform_to_hex_str(download_info->hash()).c_str());

  return wrapper;
}

void
Manager::remove_controller(TrackerControllerWrapper controller) {
  assert(std::this_thread::get_id() == main_thread::thread_id());

  auto guard = std::scoped_lock(m_lock);

  // We assume there are other references to the controller, so gracefully close it.
  if (m_controllers.erase(controller) != 1)
    throw internal_error("tracker::Manager::remove_controller(...) controller not found or has multiple references.");

  for (auto& tracker : *controller.get()->tracker_list())
    remove_events(tracker.get_worker());

  LT_LOG_TRACKER_EVENTS("removed controller: info_hash:%s", utils::transform_to_hex_str(controller.info_hash()).c_str());
}

void
Manager::send_event(tracker::Tracker& tracker, TrackerParams params, tracker::TrackerState::event_enum new_event) {
  assert(std::this_thread::get_id() == main_thread::thread_id());

  auto weak_ptr = tracker.get_weak_ptr();

  tracker_thread::thread()->callback(nullptr, [weak_ptr, params, new_event]() {
      auto tracker = weak_ptr.lock();

      if (tracker == nullptr)
        return;

      tracker->send_event(params, new_event);
    });
}

void
Manager::send_scrape(tracker::Tracker& tracker, TrackerParams params) {
  assert(std::this_thread::get_id() == main_thread::thread_id());

  auto weak_ptr = tracker.get_weak_ptr();

  tracker_thread::thread()->callback(nullptr, [weak_ptr, params]() {
      auto tracker = weak_ptr.lock();

      if (tracker == nullptr)
        return;

      tracker->send_scrape(params);
    });
}

void
Manager::add_event(TrackerWorker* worker, std::function<void ()>&& event) {
  main_thread::thread()->callback(worker, std::move(event));
}

void
Manager::remove_events(torrent::TrackerWorker* worker) {
  main_thread::thread()->cancel_callback_and_wait(worker);
  tracker_thread::thread()->cancel_callback_and_wait(worker);
}

void
Manager::delete_tracker(Tracker tracker) {
  auto guard = std::scoped_lock(m_lock);

  if (m_trackers_to_delete.empty())
    tracker_thread::thread()->callback(nullptr, [this] { process_delete_trackers(); });

  m_trackers_to_delete.push_back(tracker);
}

void
Manager::delete_trackers(std::vector<Tracker>&& trackers) {
  auto guard = std::scoped_lock(m_lock);

  if (m_trackers_to_delete.empty())
    tracker_thread::thread()->callback(nullptr, [this] { process_delete_trackers(); });

  m_trackers_to_delete.insert(m_trackers_to_delete.end(), std::make_move_iterator(trackers.begin()), std::make_move_iterator(trackers.end()));
}

void
Manager::process_delete_trackers() {
  std::vector<Tracker> trackers;

  {
    auto guard = std::scoped_lock(m_lock);

    trackers = std::move(m_trackers_to_delete);
  }

  for (auto& tracker : trackers)
    tracker.get_worker()->cleanup();
}

} // namespace torrent::tracker
