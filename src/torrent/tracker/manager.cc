#include "config.h"

#include <cassert>
#include <utility>

#include "torrent/download_info.h"
#include "torrent/exceptions.h"
#include "torrent/tracker/manager.h"
#include "torrent/tracker/tracker.h"
#include "torrent/utils/log.h"
#include "torrent/utils/thread.h"
#include "tracker/tracker_controller.h"
#include "tracker/tracker_list.h"
#include "tracker/tracker_worker.h"

#define LT_LOG_TRACKER_EVENTS(log_fmt, ...)                             \
  lt_log_print_subsystem(LOG_TRACKER_EVENTS, "tracker::manager", log_fmt, __VA_ARGS__);

namespace torrent::tracker {

Manager::Manager(utils::Thread* main_thread, utils::Thread* tracker_thread) :
  m_main_thread(main_thread),
  m_tracker_thread(tracker_thread) {

  if (m_main_thread == nullptr)
    throw internal_error("tracker::Manager::Manager(...) main_thread is null.");

  if (m_tracker_thread == nullptr)
    throw internal_error("tracker::Manager::Manager(...) tracker_thread is null.");
}

TrackerControllerWrapper
Manager::add_controller(DownloadInfo* download_info, std::shared_ptr<TrackerController> controller) {
  assert(std::this_thread::get_id() == m_main_thread->thread_id());

  if (download_info->hash() == HashString::new_zero())
    throw internal_error("tracker::Manager::add(...) invalid info_hash.");

  auto lock = std::scoped_lock(m_lock);

  auto wrapper = TrackerControllerWrapper(download_info->hash(), std::move(controller));
  auto result  = m_controllers.insert(wrapper);

  if (!result.second)
    throw internal_error("tracker::Manager::add_controller(...) controller already exists.");

  LT_LOG_TRACKER_EVENTS("added controller: info_hash:%s", hash_string_to_hex_str(download_info->hash()).c_str());

  return wrapper;
}

void
Manager::remove_controller(TrackerControllerWrapper controller) {
  assert(std::this_thread::get_id() == m_main_thread->thread_id());

  auto lock = std::scoped_lock(m_lock);

  // We assume there are other references to the controller, so gracefully close it.
  if (m_controllers.erase(controller) != 1)
    throw internal_error("tracker::Manager::remove_controller(...) controller not found or has multiple references.");

  for (auto& tracker : *controller.get()->tracker_list())
    remove_events(tracker.get_worker());

  LT_LOG_TRACKER_EVENTS("removed controller: info_hash:%s", hash_string_to_hex_str(controller.info_hash()).c_str());
}

void
Manager::send_event(tracker::Tracker& tracker, tracker::TrackerState::event_enum new_event) {
  assert(std::this_thread::get_id() == m_main_thread->thread_id());

  // TODO: Currently executing in main thread, but should be in tracker thread.
  tracker.get_worker()->send_event(new_event);
}

void
Manager::send_scrape(tracker::Tracker& tracker) {
  assert(std::this_thread::get_id() == m_main_thread->thread_id());

  // TODO: Currently executing in main thread, but should be in tracker thread.
  tracker.get_worker()->send_scrape();
}

// Events are queued by the trackers and run in the main thread.
void
Manager::add_event(torrent::TrackerWorker* tracker_worker, std::function<void()> event) {
  m_main_thread->callback(tracker_worker, std::move(event));
}

void
Manager::remove_events(torrent::TrackerWorker* tracker_worker) {
  m_main_thread->cancel_callback_and_wait(tracker_worker);
  m_tracker_thread->cancel_callback_and_wait(tracker_worker);
}

} // namespace torrent::tracker
