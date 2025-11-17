#include "config.h"

#include "thread_tracker.h"

#include <cassert>

#include "torrent/exceptions.h"
#include "torrent/tracker/manager.h"
#include "utils/instrumentation.h"

namespace torrent {

std::atomic<ThreadTracker*> ThreadTracker::m_thread_tracker{nullptr};

ThreadTracker::~ThreadTracker() = default;

void
ThreadTracker::create_thread(utils::Thread* main_thread) {
  assert(m_thread_tracker == nullptr);

  m_thread_tracker = new ThreadTracker();
  m_thread_tracker.load()->m_tracker_manager = std::make_unique<tracker::Manager>(main_thread, m_thread_tracker);
}

void
ThreadTracker::destroy_thread() {
  delete m_thread_tracker;
  m_thread_tracker = nullptr;
}

ThreadTracker*
ThreadTracker::thread_tracker() {
  return m_thread_tracker;
}

void
ThreadTracker::init_thread() {
  m_state = STATE_INITIALIZED;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_TRACKER - INSTRUMENTATION_POLLING_DO_POLL;

  // m_signal_send_event = utils::Thread::self()->signal_bitfield()->add_signal([this]() {
  //   process_send_events();
  // });
}

void
ThreadTracker::cleanup_thread() {
  m_tracker_manager.reset();
}

// void
// ThreadTracker::send_event(tracker::Tracker& tracker, tracker::TrackerState::event_enum event) {
//   {
//     std::scoped_lock lock(m_send_events_lock);

//     m_send_events.erase(std::remove_if(m_send_events.begin(), m_send_events.end(),
//                                        [&tracker](const TrackerSendEvent& e) {
//                                          return e.tracker == tracker;
//                                        }),
//                         m_send_events.end());

//     m_send_events.push_back(TrackerSendEvent{tracker, event});
//   }

//   send_event_signal(m_signal_send_event);
// }

void
ThreadTracker::call_events() {
  // lt_log_print_locked(torrent::LOG_THREAD_NOTICE, "Got ThreadTracker tick.");

  // TODO: Consider moving this into timer events instead.
  if ((m_flags & flag_do_shutdown)) {
    if ((m_flags & flag_did_shutdown))
      throw internal_error("Already trigged shutdown.");

    m_flags |= flag_did_shutdown;
    throw shutdown_exception();
  }

  // TODO: Do we need to process scheduled events here?

  process_callbacks();
}

std::chrono::microseconds
ThreadTracker::next_timeout() {
  return std::chrono::microseconds(10s);
}

// void
// ThreadTracker::process_send_events() {
//   std::vector<TrackerSendEvent> events;

//   // TODO: Do we properly handle if the tracker is deleted?
//   //
//   // Should be, as we use weak_ptrs to the tracker objects, and check them before calling.
//   //
//   // However this should be possible to implement as a straight up callback, no need for special
//   // event handling.

//   // TODO: Make sure this is called in the right thread, currently main thread.

//   // TODO: Review this for proper close:

//   {
//     auto lock = std::scoped_lock(m_send_events_lock);

//     events.swap(m_send_events);
//   }

//   // TODO: Currently running send_* in main thread, should be in the tracker thread.

//   for (auto& event : events) {
//     if (event.event == tracker::TrackerState::EVENT_SCRAPE)
//       event.tracker.get_worker()->send_scrape();
//     else
//       event.tracker.get_worker()->send_event(event.event);
//   }
// }

} // namespace torrent
