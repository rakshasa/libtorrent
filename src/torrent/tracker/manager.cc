#include "config.h"

#include <cassert>
#include <utility>

#include "torrent/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/tracker/manager.h"
#include "torrent/tracker/tracker.h"
#include "torrent/utils/log.h"
#include "torrent/system/callbacks.h"
#include "torrent/system/thread.h"
#include "torrent/utils/string_manip.h"
#include "tracker/tracker_controller.h"
#include "tracker/tracker_list.h"
#include "tracker/tracker_worker.h"

#define LT_LOG_TRACKER_EVENTS(log_fmt, ...)                             \
  lt_log_print_subsystem(LOG_TRACKER_EVENTS, "tracker::manager", log_fmt, __VA_ARGS__);

// TODO: Add leak check for trackers that are requesting when deleted, yet never got moved to delete
// queue.

namespace torrent::tracker {

Manager::Manager() = default;

// This doesn't ensure newly deleted torrents finish their stopped announce in case we shut down
// immediately after, however this is an edge-case that's not worth adding complexity to handle.

Manager::~Manager() {
  assert(std::this_thread::get_id() == tracker_thread::thread_id());

  {
    auto guard = std::scoped_lock(m_lock);

    m_trackers_to_delete.insert(m_trackers_to_delete.end(), m_trackers_to_wait.begin(), m_trackers_to_wait.end());
    m_trackers_to_wait.clear();
  }

  process_delete_trackers();
}

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

  // TrackerList is already cleared by DownloadWrapper.
  // for (auto& tracker : *controller.get()->tracker_list())
  //   remove_events(tracker.get_worker());

  LT_LOG_TRACKER_EVENTS("removed controller: info_hash:%s", utils::transform_to_hex_str(controller.info_hash()).c_str());
}

void
Manager::send_event(tracker::Tracker& tracker, TrackerParams params, tracker::TrackerState::event_enum new_event) {
  assert(std::this_thread::get_id() == main_thread::thread_id());

  auto weak_ptr = tracker.get_weak_ptr();

  tracker_thread::thread()->callback(tracker.get_worker()->callback_id(), [weak_ptr, params, new_event]() {
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

  tracker_thread::thread()->callback(tracker.get_worker()->callback_id(), [weak_ptr, params]() {
      auto tracker = weak_ptr.lock();

      if (tracker == nullptr)
        return;

      tracker->send_scrape(params);
    });
}

void
Manager::add_event(std::weak_ptr<TrackerWorker> weak_ptr, std::weak_ptr<void> tl_keeper, std::function<void (Tracker&)>&& event) {
  auto tracker = tracker::Tracker::from_weak_ptr(weak_ptr);

  if (!tracker.is_valid())
    return;

  main_thread::thread()->callback(tracker.get_worker()->callback_id(), [weak_ptr, tl_keeper, event = std::move(event)]() mutable {
      auto tracker = tracker::Tracker::from_weak_ptr(weak_ptr);

      if (!tracker.is_valid())
        return;

      auto tl_keeper_shared = tl_keeper.lock();

      if (!tl_keeper_shared)
        return;

      event(tracker);
    });
}

void
Manager::add_event_or_update(std::weak_ptr<TrackerWorker> weak_ptr, std::weak_ptr<void> tl_keeper, std::function<void (Tracker&)>&& event) {
  auto tracker = tracker::Tracker::from_weak_ptr(weak_ptr);

  if (!tracker.is_valid())
    return tracker_thread::manager()->update_tracker(std::move(tracker));

  main_thread::thread()->callback(tracker.get_worker()->callback_id(), [weak_ptr, tl_keeper, event = std::move(event)]() mutable {
      auto tracker = tracker::Tracker::from_weak_ptr(weak_ptr);

      if (!tracker.is_valid())
        return;

      auto tl_keeper_shared = tl_keeper.lock();

      if (!tl_keeper_shared)
        return tracker_thread::manager()->update_tracker(std::move(tracker));

      event(tracker);
    });
}

void
Manager::update_tracker(const Tracker& tracker) {
  auto guard = std::scoped_lock(m_lock);

  // There might have been an old callback queued before tracker list got deleted, so wait for the
  // currently processing request to finish.
  //
  // TrackerWorker::remove_events() would have removed the event, so don't check.

  // if (tracker.is_requesting_not_dht_scrape())
  //   return;

  auto itr = std::find(m_trackers_to_wait.begin(), m_trackers_to_wait.end(), tracker);

  if (itr == m_trackers_to_wait.end())
    return;

  if (m_trackers_to_delete.empty())
    tracker_thread::thread()->callback([this] { process_delete_trackers(); });

  m_trackers_to_wait.erase(itr);
  m_trackers_to_delete.push_back(tracker);
}

void
Manager::delete_tracker(Tracker tracker) {
  auto guard = std::scoped_lock(m_lock);

  if (tracker.is_requesting_not_dht_scrape()) {
    m_trackers_to_wait.push_back(tracker);
    return;
  }

  if (m_trackers_to_delete.empty())
    tracker_thread::thread()->callback([this] { process_delete_trackers(); });

  m_trackers_to_delete.push_back(tracker);
}

void
Manager::delete_trackers(std::vector<Tracker>&& trackers) {
  auto guard = std::scoped_lock(m_lock);

  for (auto& tracker : trackers) {
    if (tracker.is_requesting_not_dht_scrape()) {
      m_trackers_to_wait.push_back(std::move(tracker));
      continue;
    }

    if (m_trackers_to_delete.empty())
      tracker_thread::thread()->callback([this] { process_delete_trackers(); });

    m_trackers_to_delete.push_back(std::move(tracker));
  }
}

void
Manager::process_delete_trackers() {
  assert(std::this_thread::get_id() == tracker_thread::thread_id());

  std::vector<Tracker> trackers;

  {
    auto guard = std::scoped_lock(m_lock);

    trackers = std::move(m_trackers_to_delete);
  }

  for (auto& tracker : trackers)
    tracker.get_worker()->cleanup();
}

} // namespace torrent::tracker
