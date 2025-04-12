#include "config.h"

#include <rak/timer.h>

#include "thread_tracker.h"

#include "manager.h"
#include "torrent/exceptions.h"
#include "torrent/poll.h"
#include "torrent/tracker/manager.h"
#include "torrent/utils/log.h"
#include "tracker/tracker_worker.h"
#include "utils/instrumentation.h"

namespace torrent {

ThreadTracker::ThreadTracker(utils::Thread* main_thread) :
  m_tracker_manager(std::make_unique<tracker::Manager>(main_thread)) {
}

ThreadTracker::~ThreadTracker() = default;

void
ThreadTracker::init_thread() {
  if (!Poll::slot_create_poll())
    throw internal_error("ThreadTracker::init_thread(): Poll::slot_create_poll() not valid.");

  m_poll = std::unique_ptr<Poll>(Poll::slot_create_poll()());
  m_state = STATE_INITIALIZED;

  m_instrumentation_index = INSTRUMENTATION_POLLING_DO_POLL_TRACKER - INSTRUMENTATION_POLLING_DO_POLL;

  m_signal_send_event = thread_self->signal_bitfield()->add_signal([this]() {
    process_send_events();
  });
}

void
ThreadTracker::send_event(tracker::Tracker& tracker, tracker::TrackerState::event_enum event) {
  std::lock_guard<std::mutex> guard(m_send_events_lock);

  m_send_events.erase(std::remove_if(m_send_events.begin(),
                                     m_send_events.end(),
                                     [&tracker](const TrackerSendEvent& e) {
                                       return e.tracker == tracker;
                                     }),
                      m_send_events.end());

  m_send_events.push_back(TrackerSendEvent{tracker, event});

  send_event_signal(m_signal_send_event);
}

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

int64_t
ThreadTracker::next_timeout_usec() {
  return rak::timer::from_minutes(60).round_seconds().usec();
}

void
ThreadTracker::process_send_events() {
  std::vector<TrackerSendEvent> events;

  {
    std::lock_guard<std::mutex> guard(m_send_events_lock);

    events.swap(m_send_events);
  }

  for (auto& event : events) {
    if (event.event == tracker::TrackerState::EVENT_SCRAPE)
      event.tracker.get_worker()->send_scrape();
    else
      event.tracker.get_worker()->send_event(event.event);
  }
}

}
