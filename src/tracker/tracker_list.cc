#include "config.h"

#include "tracker/tracker_list.h"

#include <functional>
#include <random>

#include "torrent/exceptions.h"
#include "torrent/download_info.h"
#include "torrent/runtime/network_manager.h"
#include "torrent/tracker/manager.h"
#include "torrent/tracker/tracker.h"
#include "torrent/utils/log.h"
#include "torrent/utils/option_strings.h"
#include "tracker/thread_tracker.h"
#include "tracker/tracker_dht.h"
#include "tracker/tracker_http.h"
#include "tracker/tracker_udp.h"

#define LT_LOG(log_fmt, ...)                                            \
  lt_log_print_hash(LOG_TRACKER_EVENTS, info()->info_hash(), "tracker_list", log_fmt, __VA_ARGS__);

namespace torrent {

TrackerList::TrackerList() :
  m_state(DownloadInfo::STOPPED),
  m_lifetime_keeper(std::make_shared<int>(0)) {
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
  return std::any_of(begin(), end(), [](auto& tracker) { return tracker.is_busy(); });
}

bool
TrackerList::has_active_not_dht() const {
  return std::any_of(begin(), end(), [](auto& tracker) { return tracker.is_requesting_not_dht(); });
}

bool
TrackerList::has_active_not_dht_scrape_disownable() const {
  return std::any_of(begin(), end(), [](auto& tracker) { return tracker.is_requesting_not_dht_scrape_disownable(); });
}

bool
TrackerList::has_active_not_scrape() const {
  return std::any_of(begin(), end(), [](auto& tracker) { return tracker.is_busy_not_scrape(); });
}

bool
TrackerList::has_active_in_group(uint32_t group) const {
  return std::any_of(begin_group(group), end_group(group), [](auto& tracker) { return tracker.is_busy(); });
}

bool
TrackerList::has_active_not_scrape_in_group(uint32_t group) const {
  return std::any_of(begin_group(group), end_group(group), [](auto& tracker) { return tracker.is_busy_not_scrape(); });
}

bool
TrackerList::has_usable() const {
  return std::any_of(begin(), end(), [](auto& tracker) { return tracker.is_usable(); });
}

void
TrackerList::clear() {
  m_lifetime_keeper.reset();

  // Make sure the tracker_list is cleared before the trackers are deleted.
  auto trackers = std::move(*static_cast<base_type*>(this));

  ThreadTracker::thread_tracker()->tracker_manager()->delete_trackers(std::move(trackers));
}

void
TrackerList::clear_stats() {
  std::for_each(begin(), end(), std::mem_fn(&tracker::Tracker::clear_stats));
}

void
TrackerList::send_event(tracker::Tracker& tracker, tracker::TrackerState::event_enum event) {
  if (!tracker.is_valid())
    throw internal_error("TrackerList::send_event(...) tracker is invalid.");

  if (find(tracker) == end())
    throw internal_error("TrackerList::send_event(...) tracker not found.");

  if (!tracker.is_usable() || event == tracker::TrackerState::EVENT_SCRAPE)
    return;

  if (tracker.is_busy()) {
    if (tracker.state().latest_event() == event)
      return;

    if (tracker.state().latest_event() != tracker::TrackerState::EVENT_SCRAPE) {
      if (event == tracker::TrackerState::EVENT_NONE)
        return;
    }
  }

  LT_LOG("sending %s : requester:%p url:%s",
         option_as_string(OPTION_TRACKER_EVENT, event), tracker.get_worker(), tracker.url().c_str());

  tracker::TrackerParams params;

  params.numwant            = m_numwant;
  params.uploaded_adjusted  = m_info->uploaded_adjusted();
  params.completed_adjusted = m_info->completed_adjusted();
  params.download_left      = m_info->slot_left()();

  ThreadTracker::thread_tracker()->tracker_manager()->send_event(tracker, params, event);
}

void
TrackerList::send_scrape(tracker::Tracker& tracker) {
  if (!tracker.is_valid())
    throw internal_error("TrackerList::send_scrape(...) tracker is invalid.");

  if (find(tracker) == end())
    throw internal_error("TrackerList::send_scrape(...) tracker not found.");

  if (tracker.is_busy() || !tracker.is_usable())
    return;

  if (!tracker.is_scrapable())
    return;

  auto timeout = std::chrono::seconds(tracker.state().scrape_time_last()) + 600s;

  if (timeout > this_thread::cached_time())
    return;

  LT_LOG("sending scrape : requester:%p url:%s", tracker.get_worker(), tracker.url().c_str());

  tracker::TrackerParams params;

  params.numwant            = m_numwant;
  params.uploaded_adjusted  = m_info->uploaded_adjusted();
  params.completed_adjusted = m_info->completed_adjusted();
  params.download_left      = m_info->slot_left()();

  ThreadTracker::thread_tracker()->tracker_manager()->send_scrape(tracker, params);
}

TrackerList::iterator
TrackerList::insert(const tracker::Tracker& tracker) {
  auto itr = base_type::insert(end_group(tracker.group()), tracker);

  // These slots are called from within the worker thread, so we need to
  // use proper signal passing to the main thread.
  //
  // The weak_ptr should always return a valid shared_ptr, as the tracker thread will hold a
  // shared_ptr.

  auto weak_ptr        = itr->get_weak_ptr();
  auto worker          = itr->get_worker();
  auto tracker_manager = ThreadTracker::thread_tracker()->tracker_manager();
  auto lifetime_keeper = std::weak_ptr<void>(m_lifetime_keeper);

  // TODO: enable/disable slots are called from main-thread, and should not use events? Or rather,
  // remove them from tracker::Tracker.

  worker->m_slot_enabled = [this, tracker_manager, lifetime_keeper, weak_ptr, worker]() {
      tracker_manager->add_event(worker, [this, lifetime_keeper, weak_ptr]() {
          if (!m_slot_tracker_enabled)
            return;

          auto tracker_shared_ptr = weak_ptr.lock();

          if (tracker_shared_ptr && lifetime_keeper.lock())
            m_slot_tracker_enabled(tracker::Tracker(std::move(tracker_shared_ptr)));
        });
    };

  worker->m_slot_disabled = [this, tracker_manager, lifetime_keeper, weak_ptr, worker]() {
      tracker_manager->add_event(worker, [this, lifetime_keeper, weak_ptr]() {
          if (!m_slot_tracker_disabled)
            return;

          auto tracker_shared_ptr = weak_ptr.lock();

          if (tracker_shared_ptr && lifetime_keeper.lock())
            m_slot_tracker_disabled(tracker::Tracker(std::move(tracker_shared_ptr)));
        });
    };

  worker->m_slot_close = [tracker_manager, worker]() {
      tracker_manager->remove_events(worker);
    };

  worker->m_slot_success = [this, tracker_manager, lifetime_keeper, weak_ptr, worker](AddressList&& l) {
      tracker_manager->add_event(worker, [this, weak_ptr, lifetime_keeper, l = std::move(l)]() {
          if (!m_slot_success)
            return;

          auto tracker_shared_ptr = weak_ptr.lock();

          if (tracker_shared_ptr && lifetime_keeper.lock())
            receive_success(tracker::Tracker(std::move(tracker_shared_ptr)), const_cast<AddressList*>(&l));
        });
    };

  worker->m_slot_failure = [this, tracker_manager, lifetime_keeper, weak_ptr, worker](const std::string& msg) {
      tracker_manager->add_event(worker, [this, weak_ptr, lifetime_keeper, msg]() {
          if (!m_slot_failed)
            return;

          auto tracker_shared_ptr = weak_ptr.lock();

          if (tracker_shared_ptr && lifetime_keeper.lock())
            receive_failed(tracker::Tracker(std::move(tracker_shared_ptr)), msg);
        });
    };

  worker->m_slot_scrape_success = [this, tracker_manager, lifetime_keeper, weak_ptr, worker]() {
      tracker_manager->add_event(worker, [this, lifetime_keeper, weak_ptr]() {
          if (!m_slot_scrape_success)
            return;

          auto tracker_shared_ptr = weak_ptr.lock();

          if (tracker_shared_ptr && lifetime_keeper.lock())
            receive_scrape_success(tracker::Tracker(std::move(tracker_shared_ptr)));
        });
    };

  worker->m_slot_scrape_failure = [this, tracker_manager, lifetime_keeper, weak_ptr, worker](const std::string& msg) {
      tracker_manager->add_event(worker, [this, weak_ptr, lifetime_keeper, msg]() {
          if (!m_slot_scrape_failed)
            return;

          auto tracker_shared_ptr = weak_ptr.lock();

          if (tracker_shared_ptr && lifetime_keeper.lock())
            receive_scrape_failed(tracker::Tracker(std::move(tracker_shared_ptr)), msg);
        });
    };

  worker->m_slot_new_peers = [this, tracker_manager, lifetime_keeper, weak_ptr, worker](AddressList&& l) {
      tracker_manager->add_event(worker, [this, weak_ptr, lifetime_keeper, l = std::move(l)]() {
          if (!m_slot_new_peers)
            return;

          auto tracker_shared_ptr = weak_ptr.lock();

          if (tracker_shared_ptr && lifetime_keeper.lock())
            receive_new_peers(tracker::Tracker(std::move(tracker_shared_ptr)), const_cast<AddressList*>(&l));
        });
    };

  LT_LOG("added tracker : requester:%p group:%u url:%s", worker, itr->group(), itr->url().c_str());

  if (m_slot_tracker_enabled)
    m_slot_tracker_enabled(tracker);

  return itr;
}

// TODO: Use proper flags for insert options.
void
TrackerList::insert_url(unsigned int group, const std::string& url, bool extra_tracker) {
  int flags = tracker::TrackerState::flag_enabled;

  if (extra_tracker)
    flags |= tracker::TrackerState::flag_extra_tracker;

  TrackerInfo tracker_info;

  tracker_info.info_hash       = m_info->hash();
  tracker_info.obfuscated_hash = m_info->hash_obfuscated();
  tracker_info.local_id        = m_info->local_id();
  tracker_info.url             = url;
  tracker_info.group           = group;
  tracker_info.key             = m_key;

  std::shared_ptr<TrackerWorker> worker;

  if (std::strncmp("http://", url.c_str(), 7) == 0 ||
      std::strncmp("https://", url.c_str(), 8) == 0) {
    worker = std::make_shared<TrackerHttp>(tracker_info, flags);

  } else if (std::strncmp("udp://", url.c_str(), 6) == 0) {
    worker = std::make_shared<tracker::TrackerUdp>(tracker_info, flags);

  } else if (std::strncmp("dht://", url.c_str(), 6) == 0 && runtime::network_manager()->is_dht_valid()) {
    // TODO: Don't check TrackerDht::is_allowed().
    worker = std::make_shared<TrackerDht>(tracker_info, flags);

  } else {
    LT_LOG("could find matching tracker protocol : url:%s", url.c_str());

    if (extra_tracker)
      throw torrent::input_error("could find matching tracker protocol (url:" + url + ")");

    return;
  }

  insert(tracker::Tracker(std::move(worker)));
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
  auto first = begin_group(group);

  if (first == end() || first->group() != group)
    return;

  auto last = first;
  while (last != end() && last->group() == group)
    ++last;

  std::rotate(first, std::next(first), last);
}

TrackerList::iterator
TrackerList::promote(iterator itr) {
  auto first = begin_group((*itr).group());

  if (first == end())
    throw internal_error("torrent::TrackerList::promote(...) Could not find beginning of group.");

  std::iter_swap(first, itr);
  return first;
}

void
TrackerList::randomize_group_entries() {
  static std::random_device rd;
  static std::mt19937       rng(rd());

  auto itr = begin();

  while (itr != end()) {
    auto tmp = end_group((*itr).group());
    std::shuffle(itr, tmp, rng);

    itr = tmp;
  }
}

void
TrackerList::receive_success(tracker::Tracker tracker, AddressList* l) {
  LT_LOG("received %zu peers : requester:%p group:%u url:%s",
         l->size(), tracker.get_worker(), tracker.group(), tracker.url().c_str());

  auto itr = find(tracker);

  if (itr == end())
    throw internal_error("TrackerList::receive_success(...) called but the iterator is invalid.");

  if (tracker.is_busy())
    throw internal_error("TrackerList::receive_success(...) called but the tracker is still busy.");

  // Promote the tracker to the front of the group since it was
  // successfull.
  promote(itr);

  l->sort_and_unique();

  // TODO: Update staate in TrackerWorker.

  {
    auto guard = tracker.get_worker()->lock_guard();
    tracker.get_worker()->state().m_success_time_last = this_thread::cached_seconds().count();
    tracker.get_worker()->state().m_success_counter++;
    tracker.get_worker()->state().m_failed_counter = 0;
    tracker.get_worker()->state().m_latest_sum_peers = l->size();
  }

  if (!m_slot_success)
    return;

  auto new_peers = m_slot_success(tracker, l);

  {
    auto guard = tracker.get_worker()->lock_guard();
    tracker.get_worker()->state().m_latest_new_peers = new_peers + tracker.get_worker()->state().m_latest_new_peers_delta;
    tracker.get_worker()->state().m_latest_new_peers_delta = 0;
  }
}

void
TrackerList::receive_failed(tracker::Tracker tracker, const std::string& msg) {
  LT_LOG("received failure : requester:%p group:%u url:%s msg:'%s'",
         tracker.get_worker(), tracker.group(), tracker.url().c_str(), msg.c_str());

  auto itr = find(tracker);

  if (itr == end())
    throw internal_error("TrackerList::receive_failed(...) called but the iterator is invalid.");

  if (tracker.is_busy())
    throw internal_error("TrackerList::receive_failed(...) called but the tracker is still busy.");

  {
    auto guard = tracker.get_worker()->lock_guard();
    tracker.get_worker()->state().m_failed_time_last = this_thread::cached_seconds().count();
    tracker.get_worker()->state().m_failed_counter++;
    tracker.get_worker()->state().m_latest_new_peers_delta = 0;
  }

  if (m_slot_failed)
    m_slot_failed(tracker, msg);
}

void
TrackerList::receive_scrape_success(tracker::Tracker tracker) {
  LT_LOG("received scrape success : requester:%p group:%u url:%s",
         tracker.get_worker(), tracker.group(), tracker.url().c_str());

  auto itr = find(tracker);

  if (itr == end())
    throw internal_error("TrackerList::receive_scrape_success(...) called but the iterator is invalid.");

  if (tracker.is_busy())
    throw internal_error("TrackerList::receive_scrape_success(...) called but the tracker is still busy.");

  {
    auto guard = tracker.get_worker()->lock_guard();
    tracker.get_worker()->state().m_scrape_time_last = this_thread::cached_seconds().count();
    tracker.get_worker()->state().m_scrape_counter++;
  }

  if (m_slot_scrape_success)
    m_slot_scrape_success(tracker);
}

void
TrackerList::receive_scrape_failed(tracker::Tracker tracker, const std::string& msg) {
  LT_LOG("received scrape failure : requester:%p group:%u url:%s msg:'%s'",
         tracker.get_worker(), tracker.group(), tracker.url().c_str(), msg.c_str());

  auto itr = find(tracker);

  if (itr == end())
    throw internal_error("TrackerList::receive_scrape_failed(...) called but the iterator is invalid.");

  if (tracker.is_busy())
    throw internal_error("TrackerList::receive_scrape_failed(...) called but the tracker is still busy.");

  if (m_slot_scrape_failed)
    m_slot_scrape_failed(tracker, msg);
}

void
TrackerList::receive_new_peers(tracker::Tracker tracker, AddressList* l) {
  LT_LOG("received %zu new peers : requester:%p group:%u url:%s",
         l->size(), tracker.get_worker(), tracker.group(), tracker.url().c_str());

  auto itr = find(tracker);

  if (itr == end())
    throw internal_error("TrackerList::receive_new_peers(...) called but the iterator is invalid.");

  l->sort_and_unique();

  auto new_peers = m_slot_new_peers(l);

  {
    auto guard = tracker.get_worker()->lock_guard();
    tracker.get_worker()->state().m_latest_new_peers_delta += new_peers;
  }
}

} // namespace torrent
