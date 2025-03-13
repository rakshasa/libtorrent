#include "config.h"

#include "torrent/tracker/manager.h"

#include <cassert>
#include <utility>

#include "thread_main.h"
#include "src/manager.h"
#include "tracker/tracker_worker.h"
#include "torrent/exceptions.h"
#include "torrent/download_info.h"
#include "torrent/tracker_controller.h"
#include "torrent/tracker/tracker.h"
#include "torrent/utils/log.h"

#define LT_LOG_TRACKER_EVENTS(log_fmt, ...)                             \
  lt_log_print_subsystem(LOG_TRACKER_EVENTS, "tracker::manager", log_fmt, __VA_ARGS__);

namespace torrent::tracker {

Manager::Manager() {
  m_signal_process_events = thread_main->signal_bitfield()->add_signal([this]() {
      return process_events();
    });
}

TrackerControllerWrapper
Manager::add_controller(DownloadInfo* download_info, TrackerController* controller) {
  assert(std::this_thread::get_id() == torrent::thread_main->thread_id());

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
  assert(std::this_thread::get_id() == torrent::thread_main->thread_id());

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

    m_tracker_list_events.erase(itr, m_tracker_list_events.end());
  }

  LT_LOG_TRACKER_EVENTS("removed controller: info_hash:%s", hash_string_to_hex_str(controller.info_hash()).c_str());
}

void
Manager::send_event(tracker::Tracker& tracker, tracker::TrackerState::event_enum new_event) {
  assert(std::this_thread::get_id() == torrent::thread_main->thread_id());

  // TODO: Currently executing in main thread, but should be in tracker thread.
  tracker.get_worker()->send_event(new_event);
}

void
Manager::add_event(TrackerList* tracker_list, std::function<void()> event) {
  assert(m_signal_process_events != ~0u);

  std::lock_guard<std::mutex> guard(m_events_lock);

  m_tracker_list_events.push_back(TrackerListEvent{tracker_list, std::move(event)});

  torrent::thread_main->send_event_signal(m_signal_process_events);
}

void
Manager::process_events() {
  assert(std::this_thread::get_id() == torrent::thread_main->thread_id());

  std::vector<TrackerListEvent> events;

  {
    std::lock_guard<std::mutex> guard(m_events_lock);

    events.swap(m_tracker_list_events);
  }

  for (auto& event : events) {
    event.event();
  }
}


} // namespace torrent
