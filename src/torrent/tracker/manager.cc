#include "config.h"

#include "torrent/tracker/manager.h"

#include <cassert>

#include "src/manager.h"
#include "torrent/exceptions.h"
#include "torrent/download_info.h"
#include "torrent/tracker_controller.h"
#include "torrent/tracker/tracker.h"
#include "torrent/utils/log.h"

#define LT_LOG_TRACKER_EVENTS(log_fmt, ...)                             \
  lt_log_print_subsystem(LOG_TRACKER_EVENTS, "tracker::manager", log_fmt, __VA_ARGS__);

namespace torrent::tracker {

TrackerControllerWrapper
Manager::add_controller(DownloadInfo* download_info, TrackerController* controller) {
  if (download_info->hash() == HashString::new_zero())
    throw internal_error("tracker::Manager::add(...) invalid info_hash.");

  std::lock_guard<std::mutex> guard(m_lock);

  auto wrapper = TrackerControllerWrapper(download_info->hash(), std::shared_ptr<TrackerController>(controller));
  auto result  = m_controllers.insert(wrapper);

  if (!result.second)
    throw internal_error("tracker::Manager::add_controller(...) controller already exists.");

  LT_LOG_TRACKER_EVENTS("added controller: info_hash:%s", hash_string_to_hex_str(download_info->hash()).c_str());

  return wrapper;
}

void
Manager::remove_controller(TrackerControllerWrapper controller) {
  std::lock_guard<std::mutex> guard(m_lock);

  // We assume there are other references to the controller, so gracefully close it.
  if (m_controllers.erase(controller) != 1)
    throw internal_error("tracker::Manager::remove_controller(...) controller not found or has multiple references.");

  {
    std::lock_guard<std::mutex> events_guard(m_events_lock);

    auto itr = std::remove_if(m_tracker_list_events.begin(),
                              m_tracker_list_events.end(),
                              [tl = controller.get()->tracker_list()](const TrackerListEvent& event) {
                                return event.tracker_list == tl;
                              });

    m_tracker_list_events.erase(itr);
  }

  LT_LOG_TRACKER_EVENTS("removed controller: info_hash:%s", hash_string_to_hex_str(controller.info_hash()).c_str());
}

void
Manager::add_event(TrackerList* tracker_list, std::function<void()> event) {
  assert(m_main_thread_signal != ~0u);

  std::lock_guard<std::mutex> guard(m_events_lock);

  m_tracker_list_events.push_back(TrackerListEvent{tracker_list, event});

  torrent::manager->thread_main()->send_event_signal(m_main_thread_signal);
}

void
Manager::process_events() {
  std::vector<TrackerListEvent> events;

  {
    std::lock_guard<std::mutex> guard(m_events_lock);

    events.swap(m_tracker_list_events);
  }

  // TODO: Check if the tracker list is still valid.
  //
  // We need to add a list of tl's removed since the above lock guard, and check it.
  //
  // Rather, we verify that we're running in the main thread for add/remove_controller and process_events.

  for (auto& event : events) {
    event.event();
  }
}


} // namespace torrent
