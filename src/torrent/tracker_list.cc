#include "config.h"

#include <functional>
#include <random>

#include "net/address_list.h"
#include "torrent/exceptions.h"
#include "torrent/download_info.h"
#include "torrent/tracker_list.h"
#include "torrent/tracker/manager.h"
#include "torrent/tracker/tracker.h"
#include "torrent/utils/log.h"
#include "torrent/utils/option_strings.h"
#include "tracker/thread_tracker.h"
#include "tracker/tracker_dht.h"
#include "tracker/tracker_http.h"
#include "tracker/tracker_udp.h"

#include "globals.h"

#define LT_LOG_TRACKER(log_level, log_fmt, ...)                         \
  lt_log_print_info(LOG_TRACKER_##log_level, info(), "tracker_list", log_fmt, __VA_ARGS__);

namespace torrent {

TrackerList::TrackerList() :
  m_state(DownloadInfo::STOPPED) {
}

TrackerList::~TrackerList() {
  m_slot_success = nullptr;
  m_slot_failed = nullptr;
  m_slot_scrape_success = nullptr;
  m_slot_scrape_failed = nullptr;
  m_slot_tracker_enabled = nullptr;
  m_slot_tracker_disabled = nullptr;
}

bool
TrackerList::has_active() const {
  return std::any_of(begin(), end(), std::mem_fn(&tracker::Tracker::is_busy));
}

bool
TrackerList::has_active_not_scrape() const {
  return std::any_of(begin(), end(), std::mem_fn(&tracker::Tracker::is_busy_not_scrape));
}

bool
TrackerList::has_active_in_group(uint32_t group) const {
  return std::any_of(begin_group(group), end_group(group), std::mem_fn(&tracker::Tracker::is_busy));
}

bool
TrackerList::has_active_not_scrape_in_group(uint32_t group) const {
  return std::any_of(begin_group(group), end_group(group), std::mem_fn(&tracker::Tracker::is_busy_not_scrape));
}

bool
TrackerList::has_usable() const {
  return std::any_of(begin(), end(), std::mem_fn(&tracker::Tracker::is_usable));
}

void
TrackerList::close_all_excluding(int event_bitmap) {
  for (auto tracker : *this) {
    if ((event_bitmap & (1 << tracker.state().latest_event())))
      continue;

    tracker.get_worker()->close();
  }
}

void
TrackerList::disown_all_including(int event_bitmap) {
  for (auto& tracker : *this) {
    if ((event_bitmap & (1 << tracker.state().latest_event())))
      tracker.get_worker()->disown();
  }
}

void
TrackerList::clear() {
  // Make sure the tracker_list is cleared before the trackers are deleted.
  auto list = std::move(*(base_type*)this);
}

void
TrackerList::clear_stats() {
  std::for_each(begin(), end(), [](auto tracker) { tracker.clear_stats(); });
}

void
TrackerList::send_event(tracker::Tracker& tracker, tracker::TrackerState::event_enum event) {
  if (!tracker.is_usable() || event == tracker::TrackerState::EVENT_SCRAPE)
    return;

  // TODO: Should always send if we are trying to send a started/completed event.

  if (tracker.is_busy() && tracker.state().latest_event() != tracker::TrackerState::EVENT_SCRAPE)
    return;

  tracker.get_worker()->send_event(event);

  LT_LOG_TRACKER(INFO, "sending '%s (group:%u url:%s)",
                 option_as_string(OPTION_TRACKER_EVENT, event),
                 tracker.group(), tracker.url().c_str());
}

void
TrackerList::send_scrape(tracker::Tracker& tracker) {
  if (tracker.is_busy() || !tracker.is_usable())
    return;

  if (!tracker.is_scrapable())
    return;

  if (rak::timer::from_seconds(tracker.state().scrape_time_last()) + rak::timer::from_seconds(10 * 60) > cachedTime )
    return;

  tracker.get_worker()->send_scrape();

  LT_LOG_TRACKER(INFO, "sending scrape (group:%u url:%s)",
                 tracker.group(), tracker.url().c_str());
}

TrackerList::iterator
TrackerList::insert(unsigned int group, const tracker::Tracker& tracker) {
  iterator itr = base_type::insert(end_group(group), tracker);

  itr->set_group(group);

  // These slots are called from within the worker thread, so we need to
  // use proper signal passing to the main thread.

  // TODO: When a tracker is sent to tracker thread to do a request, it needs to hold the shared_ptr
  // for the duration of the request.

  // TODO: TrackerList should be a shared_ptr held by DownloadMain, and send_* should pass through
  // tracker::Manager, which will hold the  weak_ptr and collect results. It should poke signal main
  // thread it has work and if weak_ptr locks it performs the work.
  //
  // This means we can remove the slots below and tracker just need to have tracker::Manager*.

  // The weak_ptr should always return a valid shared_ptr, as the tracker thread will hold a
  // shared_ptr.

  auto weak_tracker = std::weak_ptr<TrackerWorker>(itr->m_worker);

  itr->m_worker->m_slot_enabled = [this, weak_tracker, worker = itr->m_worker.get()]() {
      thread_tracker->tracker_manager()->add_event(worker, [this, weak_tracker]() {
          if (!m_slot_tracker_enabled)
            return;

          auto tracker_shared_ptr = weak_tracker.lock();

          if (tracker_shared_ptr)
            m_slot_tracker_enabled(tracker::Tracker(std::move(tracker_shared_ptr)));
        });
    };

  itr->m_worker->m_slot_disabled = [this, weak_tracker, worker = itr->m_worker.get()]() {
      thread_tracker->tracker_manager()->add_event(worker, [this, weak_tracker]() {
          if (!m_slot_tracker_disabled)
            return;

          auto tracker_shared_ptr = weak_tracker.lock();

          if (tracker_shared_ptr)
            m_slot_tracker_disabled(tracker::Tracker(std::move(tracker_shared_ptr)));
        });
    };

  itr->m_worker->m_slot_close = [worker = itr->m_worker.get()]() {
      thread_tracker->tracker_manager()->remove_events(worker);
    };

  itr->m_worker->m_slot_success = [this, weak_tracker, worker = itr->m_worker.get()](AddressList&& l) {
      thread_tracker->tracker_manager()->add_event(worker, [this, weak_tracker, l = std::move(l)]() {
          if (!m_slot_success)
            return;

          auto tracker_shared_ptr = weak_tracker.lock();

          if (tracker_shared_ptr)
            receive_success(tracker::Tracker(std::move(tracker_shared_ptr)), (AddressList*)&l);
        });
    };

  itr->m_worker->m_slot_failure = [this, weak_tracker, worker = itr->m_worker.get()](const std::string& msg) {
      thread_tracker->tracker_manager()->add_event(worker, [this, weak_tracker, msg]() {
          if (!m_slot_failed)
            return;

          auto tracker_shared_ptr = weak_tracker.lock();

          if (tracker_shared_ptr)
            receive_failed(tracker::Tracker(std::move(tracker_shared_ptr)), msg);
        });
    };

  itr->m_worker->m_slot_scrape_success = [this, weak_tracker, worker = itr->m_worker.get()]() {
      thread_tracker->tracker_manager()->add_event(worker, [this, weak_tracker]() {
          if (!m_slot_scrape_success)
            return;

          auto tracker_shared_ptr = weak_tracker.lock();

          if (tracker_shared_ptr)
            receive_scrape_success(tracker::Tracker(std::move(tracker_shared_ptr)));
        });
    };

  itr->m_worker->m_slot_scrape_failure = [this, weak_tracker, worker = itr->m_worker.get()](const std::string& msg) {
      thread_tracker->tracker_manager()->add_event(worker, [this, weak_tracker, msg]() {
          if (!m_slot_scrape_failed)
            return;

          auto tracker_shared_ptr = weak_tracker.lock();

          if (tracker_shared_ptr)
            receive_scrape_failed(tracker::Tracker(std::move(tracker_shared_ptr)), msg);
        });
    };

  itr->m_worker->m_slot_parameters = [this]() -> TrackerParameters {
      // TODO: Lock here!

      return TrackerParameters{
        .numwant = m_numwant,
        .uploaded_adjusted = m_info->uploaded_adjusted(),
        .completed_adjusted = m_info->completed_adjusted(),
        .download_left = m_info->slot_left()()
      };
    };

  LT_LOG_TRACKER(INFO, "added tracker (group:%i url:%s)", group, itr->m_worker->info().url.c_str());

  if (m_slot_tracker_enabled)
    m_slot_tracker_enabled(tracker);

  return itr;
}

// TODO: Use proper flags for insert options.
void
TrackerList::insert_url(unsigned int group, const std::string& url, bool extra_tracker) {
  TrackerWorker* worker;

  int flags = tracker::TrackerState::flag_enabled;

  if (extra_tracker)
    flags |= tracker::TrackerState::flag_extra_tracker;

  auto tracker_info = TrackerInfo{
    .info_hash = m_info->hash(),
    .obfuscated_hash = m_info->hash_obfuscated(),
    .local_id = m_info->local_id(),
    .url = url,
    .key = m_key
  };

  if (std::strncmp("http://", url.c_str(), 7) == 0 ||
      std::strncmp("https://", url.c_str(), 8) == 0) {
    worker = new TrackerHttp(tracker_info, flags);

  } else if (std::strncmp("udp://", url.c_str(), 6) == 0) {
    worker = new TrackerUdp(tracker_info, flags);

  } else if (std::strncmp("dht://", url.c_str(), 6) == 0 && TrackerDht::is_allowed()) {
    worker = new TrackerDht(tracker_info, flags);

  } else {
    LT_LOG_TRACKER(WARN, "could find matching tracker protocol (url:%s)", url.c_str());

    if (extra_tracker)
      throw torrent::input_error("could find matching tracker protocol (url:" + url + ")");

    return;
  }

  insert(group, tracker::Tracker(std::shared_ptr<TrackerWorker>(worker)));
}

TrackerList::iterator
TrackerList::find_url(const std::string& url) {
  return std::find_if(begin(), end(), [&url](const auto& tracker) { return tracker.url() == url; });
}

TrackerList::iterator
TrackerList::find_next_to_request(iterator itr) {
  auto preferred = itr = std::find_if(itr, end(), std::mem_fn(&tracker::Tracker::can_request_state));

  if (preferred == end())
    return end();

  // TODO: Get only the required state values.
  auto preferred_state = (*preferred).state();

  if (preferred_state.failed_counter() == 0)
    return preferred;

  while (++itr != end()) {
    if (!(*itr).can_request_state())
      continue;

    auto itr_state = (*itr).state();

    if (itr_state.failed_counter() != 0) {
      if (itr_state.failed_time_next() < preferred_state.failed_time_next()) {
        preferred = itr;
        preferred_state = (*preferred).state();
      }

    } else {
      if (itr_state.success_time_next() < preferred_state.failed_time_next()) {
        preferred = itr;
        preferred_state = (*preferred).state();
      }

      break;
    }
  }

  return preferred;
}

TrackerList::iterator
TrackerList::begin_group(unsigned int group) {
  return std::find_if(begin(), end(), [group](const tracker::Tracker& t) { return group <= t.group(); });
}

TrackerList::const_iterator
TrackerList::begin_group(unsigned int group) const {
  return std::find_if(begin(), end(), [group](const tracker::Tracker& t) { return group <= t.group(); });
}

TrackerList::size_type
TrackerList::size_group() const {
  return !empty() ? back().group() + 1 : 0;
}

void
TrackerList::cycle_group(unsigned int group) {
  iterator itr = begin_group(group);
  iterator prev = itr;

  if (itr == end() || (*itr).group() != group)
    return;

  while (++itr != end() && (*itr).group() == group) {
    std::iter_swap(itr, prev);
    prev = itr;
  }
}

TrackerList::iterator
TrackerList::promote(iterator itr) {
  iterator first = begin_group((*itr).group());

  if (first == end())
    throw internal_error("torrent::TrackerList::promote(...) Could not find beginning of group.");

  std::swap(*first, *itr);
  return first;
}

void
TrackerList::randomize_group_entries() {
  static std::random_device rd;
  static std::mt19937       rng(rd());

  iterator itr = begin();

  while (itr != end()) {
    iterator tmp = end_group((*itr).group());
    std::shuffle(itr, tmp, rng);

    itr = tmp;
  }
}

void
TrackerList::receive_success(tracker::Tracker&& tracker, AddressList* l) {
  iterator itr = find(tracker);

  if (itr == end())
    throw internal_error("TrackerList::receive_success(...) called but the iterator is invalid.");

  if (tracker.is_busy())
    throw internal_error("TrackerList::receive_success(...) called but the tracker is still busy.");

  // Promote the tracker to the front of the group since it was
  // successfull.
  promote(itr);

  l->sort();
  l->erase(std::unique(l->begin(), l->end()), l->end());

  LT_LOG_TRACKER(INFO, "received %u peers (url:%s)", l->size(), tracker.url().c_str());

  // TODO: Update staate in TrackerWorker.

  {
    auto guard = tracker.get_worker()->lock_guard();
    tracker.get_worker()->state().m_success_time_last = cachedTime.seconds();
    tracker.get_worker()->state().m_success_counter++;
    tracker.get_worker()->state().m_failed_counter = 0;
    tracker.get_worker()->state().m_latest_sum_peers = l->size();
  }

  if (!m_slot_success)
    return;

  auto new_peers = m_slot_success(tracker, l);

  {
    auto guard = tracker.get_worker()->lock_guard();
    tracker.get_worker()->state().m_latest_new_peers = new_peers;
  }
}

void
TrackerList::receive_failed(tracker::Tracker&& tracker, const std::string& msg) {
  iterator itr = find(tracker);

  if (itr == end())
    throw internal_error("TrackerList::receive_failed(...) called but the iterator is invalid.");

  if (tracker.is_busy())
    throw internal_error("TrackerList::receive_failed(...) called but the tracker is still busy.");

  LT_LOG_TRACKER(INFO, "failed to send request to tracker (url:%s msg:%s)", tracker.url().c_str(), msg.c_str());

  {
    auto guard = tracker.get_worker()->lock_guard();
    tracker.get_worker()->state().m_failed_time_last = cachedTime.seconds();
    tracker.get_worker()->state().m_failed_counter++;
  }

  if (m_slot_failed)
    m_slot_failed(tracker, msg);
}

void
TrackerList::receive_scrape_success(tracker::Tracker&& tracker) {
  iterator itr = find(tracker);

  if (itr == end())
    throw internal_error("TrackerList::receive_scrape_success(...) called but the iterator is invalid.");

  if (tracker.is_busy())
    throw internal_error("TrackerList::receive_scrape_success(...) called but the tracker is still busy.");

  LT_LOG_TRACKER(INFO, "received scrape from tracker (url:%s)", tracker.url().c_str());

  {
    auto guard = tracker.get_worker()->lock_guard();
    tracker.get_worker()->state().m_scrape_time_last = cachedTime.seconds();
    tracker.get_worker()->state().m_scrape_counter++;
  }

  if (m_slot_scrape_success)
    m_slot_scrape_success(tracker);
}

void
TrackerList::receive_scrape_failed(tracker::Tracker&& tracker, const std::string& msg) {
  iterator itr = find(tracker);

  if (itr == end())
    throw internal_error("TrackerList::receive_scrape_failed(...) called but the iterator is invalid.");

  if (tracker.is_busy())
    throw internal_error("TrackerList::receive_scrape_failed(...) called but the tracker is still busy.");

  LT_LOG_TRACKER(INFO, "failed to send scrape to tracker (url:%s msg:%s)", tracker.url().c_str(), msg.c_str());

  if (m_slot_scrape_failed)
    m_slot_scrape_failed(tracker, msg);
}

}
