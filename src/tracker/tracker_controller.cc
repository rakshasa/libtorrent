#include "config.h"

#include "tracker/tracker_controller.h"

#include "torrent/exceptions.h"
#include "torrent/download_info.h"
#include "torrent/utils/log.h"
#include "torrent/utils/chrono.h"
#include "tracker/tracker_list.h"

#define LT_LOG_TRACKER_EVENTS(log_fmt, ...)                              \
  lt_log_print_info(LOG_TRACKER_EVENTS, m_tracker_list->info(), "tracker_controller", log_fmt, __VA_ARGS__);

namespace torrent {

// End temp hacks...

void
TrackerController::update_timeout(uint32_t seconds_to_next) {
  if (!(m_flags & flag_active))
    throw internal_error("TrackerController cannot set timeout when inactive.");

  if (seconds_to_next == 0) {
    this_thread::scheduler()->update_wait_for(&m_task_timeout, 0s);
    return;
  }

  this_thread::scheduler()->update_wait_for_ceil_seconds(&m_task_timeout, std::chrono::seconds(seconds_to_next));
}

inline tracker::TrackerState::event_enum
TrackerController::current_send_event() const {
  switch ((m_flags & mask_send)) {
  case flag_send_start:     return tracker::TrackerState::EVENT_STARTED;
  case flag_send_stop:      return tracker::TrackerState::EVENT_STOPPED;
  case flag_send_completed: return tracker::TrackerState::EVENT_COMPLETED;
  case flag_send_update:
  default:                  return tracker::TrackerState::EVENT_NONE;
  }
}

TrackerController::TrackerController(TrackerList* trackers)
  : m_tracker_list(trackers) {

  m_task_timeout.slot() = [this] { do_timeout(); };
  m_task_scrape.slot()  = [this] { do_scrape(); };
}

TrackerController::~TrackerController() {
  this_thread::scheduler()->erase(&m_task_timeout);
  this_thread::scheduler()->erase(&m_task_scrape);
}

bool
TrackerController::is_timeout_queued() const {
  return m_task_timeout.is_scheduled();
}

bool
TrackerController::is_scrape_queued() const {
  return m_task_scrape.is_scheduled();
}

int64_t
TrackerController::next_timeout() const {
  return m_task_timeout.time().count();
}

int64_t
TrackerController::next_scrape() const {
  return m_task_scrape.time().count();
}

uint32_t
TrackerController::seconds_to_next_timeout() const {
  auto timeout = m_task_timeout.time() - this_thread::cached_time();

  // LT_LOG_TRACKER_EVENTS("seconds_to_next_timeout() : %" PRId64, timeout.count());

  if (timeout <= 0s)
    return 0;

  // LT_LOG_TRACKER_EVENTS("seconds_to_next_timeout() : %" PRId64, utils::ceil_cast_seconds(timeout).count());

  return utils::ceil_cast_seconds(timeout).count();
}

uint32_t
TrackerController::seconds_to_next_scrape() const {
  auto timeout = m_task_scrape.time() - this_thread::cached_time();

  if (timeout <= 0s)
    return 0;

  return utils::ceil_cast_seconds(timeout).count();
}

void
TrackerController::manual_request([[maybe_unused]] bool request_now) {
  if (!m_task_timeout.is_scheduled())
    return;

  // Add functions to get the lowest timeout, etc...
  send_update_event();
}

void
TrackerController::scrape_request(uint32_t seconds_to_request) {
  if (seconds_to_request == 0) {
    this_thread::scheduler()->update_wait_for(&m_task_scrape, 0s);
    return;
  }

  this_thread::scheduler()->update_wait_for_ceil_seconds(&m_task_scrape, std::chrono::seconds(seconds_to_request));
}

// The send_*_event() functions tries to ensure the relevant trackers
// receive the event.
//
// When we just want more peers the start_requesting() function is
// used. This is all independent of the regular updates sent to the
// trackers.

void
TrackerController::send_start_event() {
  // This will now be 'lazy', rather than a definite event. We tell
  // the controller that a 'start' event should be sent, and it will
  // send it when the tracker controller get's enabled.

  // If the controller is already running, we insert this new event.

  // Return, or something, if already active and sending?

  if (m_flags & flag_send_start) {
    // Do we just return, or bork? At least we need to check to see
    // that there's something requesting 'start' event or fail hard.
  }

  m_flags &= ~mask_send;
  m_flags |= flag_send_start;

  if (!(m_flags & flag_active) || !m_tracker_list->has_usable()) {
    LT_LOG_TRACKER_EVENTS("sending start event : queued", 0);
    return;
  }

  // Start with requesting from the first tracker. Add timer to
  // catch when we don't get any response within the first few
  // seconds, at which point we go promiscious.

  // Do we use the old 'focus' thing?... Rather react on no reply,
  // go into promiscious.
  LT_LOG_TRACKER_EVENTS("sending start event : requesting", 0);

  close();

  // std::any_of(m_tracker_list->begin(), m_tracker_list->end(), [&](tracker::Tracker& tracker) {
  //   if (!tracker.is_usable())
  //     return false;

  //   m_tracker_list->send_event(tracker, tracker::TrackerState::EVENT_STARTED);
  //   return true;
  // });

  bool found_usable = false;

  for (auto tracker : *m_tracker_list) {
    if (!tracker.is_usable())
      continue;

    if (found_usable) {
      m_flags |= flag_promiscuous_mode;
      update_timeout(3);
      break;
    }

    m_tracker_list->send_event(tracker, tracker::TrackerState::EVENT_STARTED);
    found_usable = true;
  }
}

void
TrackerController::send_stop_event() {
  if (m_flags & flag_send_stop) {
    // Do we just return, or bork? At least we need to check to see
    // that there's something requesting 'start' event or fail hard.
  }

  m_flags &= ~mask_send;

  if (!(m_flags & flag_active) || !m_tracker_list->has_usable()) {
    LT_LOG_TRACKER_EVENTS("sending stop event : skipped stopped event as no tracker needs it", 0);
    return;
  }

  m_flags |= flag_send_stop;

  LT_LOG_TRACKER_EVENTS("sending stop event : requesting", 0);

  close();

  for (auto tracker : *m_tracker_list) {
    if (!tracker.is_in_use())
      continue;

    m_tracker_list->send_event(tracker, tracker::TrackerState::EVENT_STOPPED);
  }

  // Timer...
}

void
TrackerController::send_completed_event() {
  if (m_flags & flag_send_completed) {
    // Do we just return, or bork? At least we need to check to see
    // that there's something requesting 'start' event or fail hard.
  }

  m_flags &= ~mask_send;
  m_flags |= flag_send_completed;

  if (!(m_flags & flag_active) || !m_tracker_list->has_usable()) {
    LT_LOG_TRACKER_EVENTS("sending completed event : queued", 0);
    return;
  }

  LT_LOG_TRACKER_EVENTS("sending completed event : requesting", 0);

  close();

  for (auto tracker : *m_tracker_list) {
    if (!tracker.is_in_use())
      continue;

    m_tracker_list->send_event(tracker, tracker::TrackerState::EVENT_COMPLETED);
  }

  // Timer...
}

void
TrackerController::send_update_event() {
  if (!(m_flags & flag_active) || !m_tracker_list->has_usable())
    return;

  if ((m_flags & mask_send) && m_tracker_list->has_active())
    return;

  // We can lose a state here...
  if (!(m_flags & mask_send))
    m_flags |= flag_send_update;

  LT_LOG_TRACKER_EVENTS("sending update event : requesting", 0);

  for (auto tracker : *m_tracker_list) {
    if (!tracker.is_usable())
      continue;

    m_tracker_list->send_event(tracker, tracker::TrackerState::EVENT_NONE);
    break;
  }
}

// Currently being used by send_event, fixme.
void
TrackerController::close() {
  m_flags &= ~(flag_requesting | flag_promiscuous_mode);

  m_tracker_list->close_all();
  this_thread::scheduler()->erase(&m_task_timeout);
}

void
TrackerController::enable(int enable_flags) {
  if ((m_flags & flag_active))
    return;

  // Clearing send stop here in case we cycle disable/enable too
  // fast. In the future do this based on flags passed.
  m_flags |= flag_active;
  m_flags &= ~flag_send_stop;

  m_tracker_list->close_all_excluding((1 << tracker::TrackerState::EVENT_COMPLETED));

  if (!(enable_flags & enable_dont_reset_stats))
    m_tracker_list->clear_stats();

  LT_LOG_TRACKER_EVENTS("enabled : trackers:%zu", m_tracker_list->size());

  // Adding of the tracker requests gets done after the caller has had
  // a chance to override the default behavior.
  update_timeout(0);
}

void
TrackerController::disable() {
  if (!(m_flags & flag_active))
    return;

  // Disable other flags?...
  m_flags &= ~(flag_active | flag_requesting | flag_promiscuous_mode);

  m_tracker_list->close_all_excluding((1 << tracker::TrackerState::EVENT_STOPPED) | (1 << tracker::TrackerState::EVENT_COMPLETED));
  this_thread::scheduler()->erase(&m_task_timeout);

  LT_LOG_TRACKER_EVENTS("disabled : trackers:%zu", m_tracker_list->size());
}

void
TrackerController::start_requesting() {
  if ((m_flags & flag_requesting))
    return;

  m_flags |= flag_requesting;

  if ((m_flags & flag_active))
    update_timeout(0);

  LT_LOG_TRACKER_EVENTS("started requesting", 0);
}

void
TrackerController::stop_requesting() {
  if (!(m_flags & flag_requesting))
    return;

  m_flags &= ~flag_requesting;

  LT_LOG_TRACKER_EVENTS("stopped requesting", 0);
}

uint32_t
tracker_next_timeout(const tracker::Tracker& tracker, int controller_flags) {
  if ((controller_flags & TrackerController::flag_requesting))
    return tracker_next_timeout_promiscuous(tracker);

  int32_t                           activity_time_last;
  tracker::TrackerState::event_enum latest_event;
  int32_t                           normal_interval;

  tracker.lock_and_call_state([&](const tracker::TrackerState& state) {
    activity_time_last = state.activity_time_last();
    latest_event = state.latest_event();
    normal_interval = state.normal_interval();
  });

  if ((tracker.is_busy() && latest_event != tracker::TrackerState::EVENT_SCRAPE) ||
      !tracker.is_usable())
    return ~uint32_t();

  if ((controller_flags & TrackerController::flag_promiscuous_mode))
    return 0;

  if ((controller_flags & TrackerController::flag_send_update))
    return tracker_next_timeout_update(tracker);

  // if (tracker.success_counter() == 0 && tracker.failed_counter() == 0)
  //   return 0;

  int32_t last_activity = this_thread::cached_seconds().count() - activity_time_last;

  // TODO: Use min interval if we're requesting manual update.

  return normal_interval - std::min(last_activity, normal_interval);
}

uint32_t
tracker_next_timeout_update(const tracker::Tracker& tracker) {
  // TODO: Rewrite to be in tracker thread or atomic tracker state.
  auto tracker_state = tracker.state();

  if ((tracker.is_busy() && tracker_state.latest_event() != tracker::TrackerState::EVENT_SCRAPE) ||
      !tracker.is_usable())
    return ~uint32_t();

  // Make sure we don't request _too_ often, check last activity.
  // int32_t last_activity = this_thread::cached_seconds().count() - tracker.activity_time_last();

  return 0;
}

uint32_t
tracker_next_timeout_promiscuous(const tracker::Tracker& tracker) {
  // TODO: Rewrite to be in tracker thread or atomic tracker state.
  auto tracker_state = tracker.state();

  if ((tracker.is_busy() && tracker_state.latest_event() != tracker::TrackerState::EVENT_SCRAPE) ||
      !tracker.is_usable())
    return ~uint32_t();

  int32_t interval;

  if (tracker_state.failed_counter()) {
    interval = tracker_state.failed_time_next() - tracker_state.failed_time_last();
  } else {
    interval = tracker_state.normal_interval();
  }

  int32_t min_interval = std::max(tracker_state.min_interval(), uint32_t{300});
  int32_t use_interval = std::min(interval, min_interval);

  int32_t since_last = this_thread::cached_seconds().count() - static_cast<int32_t>(tracker_state.activity_time_last());

  lt_log_print(LOG_TRACKER_EVENTS, "tracker_next_timeout_promiscuous: min_interval:%d use_interval:%d since_last:%d",
               min_interval, use_interval, since_last);

  return std::max(use_interval - since_last, 0);
}

static TrackerList::iterator
tracker_find_preferred(TrackerList::iterator first, TrackerList::iterator last, uint32_t* next_timeout) {
  auto preferred = last;
  uint32_t preferred_time_last = ~uint32_t();

  for (; first != last; first++) {
    uint32_t tracker_timeout = tracker_next_timeout_promiscuous(*first);

    if (tracker_timeout != 0) {
      *next_timeout = std::min(tracker_timeout, *next_timeout);
      continue;
    }

    uint32_t activity_time_last;

    first->lock_and_call_state([&](const tracker::TrackerState& state) {
        activity_time_last = state.activity_time_last();
      });

    if (activity_time_last < preferred_time_last) {
      preferred = first;
      preferred_time_last = activity_time_last;
    }
  }

  return preferred;
}

void
TrackerController::do_timeout() {
  this_thread::scheduler()->erase(&m_task_timeout);

  if (!(m_flags & flag_active) || !m_tracker_list->has_usable())
    return;

  tracker::TrackerState::event_enum send_event = current_send_event();

  if ((m_flags & (flag_promiscuous_mode | flag_requesting))) {
    uint32_t next_timeout = ~uint32_t();

    auto itr = m_tracker_list->begin();

    while (itr != m_tracker_list->end()) {
      uint32_t group = itr->group();

      if (m_tracker_list->has_active_not_scrape_in_group(group)) {
        itr = m_tracker_list->end_group(group);
        continue;
      }

      auto group_end = m_tracker_list->end_group(itr->group());
      auto preferred = itr;

      // TODO: Rewrite to be in tracker thread or atomic tracker state.
      auto tracker_state = itr->state();

      if (!itr->is_usable() || tracker_state.failed_counter()) {
        // The selected tracker in the group is either disabled or not
        // reachable, try the others to find a new one to use.
        preferred = tracker_find_preferred(preferred, group_end, &next_timeout);

      } else {
        uint32_t tracker_timeout = tracker_next_timeout_promiscuous(*preferred);

        if (tracker_timeout != 0) {
          next_timeout = std::min(tracker_timeout, next_timeout);
          preferred = group_end;
        }
      }

      if (preferred != group_end)
        m_tracker_list->send_event(*preferred, send_event);

      itr = group_end;
    }

    if (next_timeout != ~uint32_t())
      update_timeout(next_timeout);

  // TODO: Send for start/completed also?
  } else {
    auto itr = m_tracker_list->find_next_to_request(m_tracker_list->begin());

    if (itr == m_tracker_list->end())
      return;

    // TODO: Rewrite to be in tracker thread or atomic tracker state.
    auto tracker_state = itr->state();

    int32_t next_timeout = tracker_state.activity_time_next();
    int32_t cached_seconds = this_thread::cached_seconds().count();

    if (next_timeout <= cached_seconds)
      m_tracker_list->send_event(*itr, send_event);
    else
      update_timeout(next_timeout - cached_seconds);
  }

  if (m_slot_timeout)
    m_slot_timeout();
}

void
TrackerController::do_scrape() {
  auto itr = m_tracker_list->begin();

  while (itr != m_tracker_list->end()) {
    uint32_t group = itr->group();

    if (m_tracker_list->has_active_in_group(group)) {
      itr = m_tracker_list->end_group(group);
      continue;
    }

    auto group_end = m_tracker_list->end_group(itr->group());

    while (itr != group_end) {
      if (itr->is_scrapable() && itr->is_usable()) {
        m_tracker_list->send_scrape(*itr);
        break;
      }

      itr++;
    }

    itr = group_end;
  }
}

uint32_t
TrackerController::receive_success(const tracker::Tracker& tracker, TrackerController::address_list* l) {
  if (!(m_flags & flag_active))
    return m_slot_success(l);

  // if (<check if we have multiple trackers to send this event to, before we declare success>) {
  m_flags &= ~(mask_send | flag_promiscuous_mode | flag_failure_mode);
  // }

  // If we still have active trackers, skip the timeout.

  // Calculate the next timeout according to a list of in-use
  // trackers, with the first timeout as the interval.

  if ((m_flags & flag_requesting))
    update_timeout(30);
  else if (!m_tracker_list->has_active()) {
    uint32_t normal_interval;

    tracker.lock_and_call_state([&](const tracker::TrackerState& state) {
        normal_interval = state.normal_interval();
      });

    // TODO: Instead find the lowest timeout, correct timeout?
    update_timeout(normal_interval);
  }

  return m_slot_success(l);
}

void
TrackerController::receive_failure(const tracker::Tracker& tracker, const std::string& msg) {
  if (!(m_flags & flag_active)) {
    m_slot_failure(msg);
    return;
  }

  // if (tracker == nullptr) {
  //   m_slot_failure(msg);
  //   return;
  // }

  int32_t failed_counter;
  int32_t success_counter;

  tracker.lock_and_call_state([&](const tracker::TrackerState& state) {
      failed_counter = state.failed_counter();
      success_counter = state.success_counter();
    });

  if (failed_counter == 1 && success_counter > 0)
    m_flags |= flag_failure_mode;

  do_timeout();
  m_slot_failure(msg);
}

void
TrackerController::receive_scrape([[maybe_unused]] const tracker::Tracker& tracker) const {
  if (!(m_flags & flag_active)) {
    return;
  }
}

uint32_t
TrackerController::receive_new_peers(address_list* l) {
  return m_slot_success(l);
}

void
TrackerController::receive_tracker_enabled(const tracker::Tracker& tb) {
  // TODO: This won't be needed if we rely only on Tracker::m_enable,
  // rather than a virtual function.
  if (!m_tracker_list->has_usable())
    return;

  if ((m_flags & flag_active)) {
    if (!m_task_timeout.is_scheduled() && !m_tracker_list->has_active()) {
      // TODO: Figure out the proper timeout to use here based on when the
      // tracker last connected, etc.
      update_timeout(0);
    }
  }

  if (m_slot_tracker_enabled)
    m_slot_tracker_enabled(tb);
}

void
TrackerController::receive_tracker_disabled(const tracker::Tracker& tb) {
  if ((m_flags & flag_active) && !m_task_timeout.is_scheduled())
    update_timeout(0);

  if (m_slot_tracker_disabled)
    m_slot_tracker_disabled(tb);
}

} // namespace torrent
