#include "config.h"

#include "tracker/tracker_list.h"

#include <functional>
#include <random>

#include "torrent/exceptions.h"
#include "torrent/download_info.h"
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
TrackerList::has_active_not_dht() const {
  return std::any_of(begin(), end(), [](const tracker::Tracker& tracker) {
      return tracker.is_busy() && tracker.type() != tracker_enum::TRACKER_DHT;
    });
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
  LT_LOG("closing all trackers with event bitmap: 0x%x", event_bitmap);

  for (auto tracker : *this) {
    if ((event_bitmap & (1 << tracker.state().latest_event())))
      continue;

    tracker.get_worker()->close();
  }
}

void
TrackerList::clear() {
  // Make sure the tracker_list is cleared before the trackers are deleted.
  auto list = std::move(*static_cast<base_type*>(this));
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

  // TODO: Should always send if we are trying to send a started/completed event.

  if (tracker.is_busy() && tracker.state().latest_event() != tracker::TrackerState::EVENT_SCRAPE)
    return;

  LT_LOG("sending %s : requester:%p url:%s",
         option_as_string(OPTION_TRACKER_EVENT, event), tracker.get_worker(), tracker.url().c_str());

  thread_tracker()->tracker_manager()->send_event(tracker, event);
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

  thread_tracker()->tracker_manager()->send_scrape(tracker);
}

TrackerList::iterator
TrackerList::insert(unsigned int group, const tracker::Tracker& tracker) {
  auto itr = base_type::insert(end_group(group), tracker);

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
  auto worker = itr->get_worker();

  worker->set_group(group);

  worker->m_slot_enabled = [this, weak_tracker, worker]() {
      thread_tracker()->tracker_manager()->add_event(worker, [this, weak_tracker]() {
          if (!m_slot_tracker_enabled)
            return;

          auto tracker_shared_ptr = weak_tracker.lock();

          if (tracker_shared_ptr)
            m_slot_tracker_enabled(tracker::Tracker(std::move(tracker_shared_ptr)));
        });
    };

  worker->m_slot_disabled = [this, weak_tracker, worker]() {
      thread_tracker()->tracker_manager()->add_event(worker, [this, weak_tracker]() {
          if (!m_slot_tracker_disabled)
            return;

          auto tracker_shared_ptr = weak_tracker.lock();

          if (tracker_shared_ptr)
            m_slot_tracker_disabled(tracker::Tracker(std::move(tracker_shared_ptr)));
        });
    };

  worker->m_slot_close = [worker]() {
      thread_tracker()->tracker_manager()->remove_events(worker);
    };

  worker->m_slot_success = [this, weak_tracker, worker](AddressList&& l) {
      thread_tracker()->tracker_manager()->add_event(worker, [this, weak_tracker, l = std::move(l)]() {
          if (!m_slot_success)
            return;

          auto tracker_shared_ptr = weak_tracker.lock();

          if (tracker_shared_ptr)
            receive_success(tracker::Tracker(std::move(tracker_shared_ptr)), const_cast<AddressList*>(&l));
        });
    };

  worker->m_slot_failure = [this, weak_tracker, worker](const std::string& msg) {
      thread_tracker()->tracker_manager()->add_event(worker, [this, weak_tracker, msg]() {
          if (!m_slot_failed)
            return;

          auto tracker_shared_ptr = weak_tracker.lock();

          if (tracker_shared_ptr)
            receive_failed(tracker::Tracker(std::move(tracker_shared_ptr)), msg);
        });
    };

  worker->m_slot_scrape_success = [this, weak_tracker, worker]() {
      thread_tracker()->tracker_manager()->add_event(worker, [this, weak_tracker]() {
          if (!m_slot_scrape_success)
            return;

          auto tracker_shared_ptr = weak_tracker.lock();

          if (tracker_shared_ptr)
            receive_scrape_success(tracker::Tracker(std::move(tracker_shared_ptr)));
        });
    };

  worker->m_slot_scrape_failure = [this, weak_tracker, worker](const std::string& msg) {
      thread_tracker()->tracker_manager()->add_event(worker, [this, weak_tracker, msg]() {
          if (!m_slot_scrape_failed)
            return;

          auto tracker_shared_ptr = weak_tracker.lock();

          if (tracker_shared_ptr)
            receive_scrape_failed(tracker::Tracker(std::move(tracker_shared_ptr)), msg);
        });
    };

  worker->m_slot_new_peers = [this, weak_tracker, worker](AddressList&& l) {
      thread_tracker()->tracker_manager()->add_event(worker, [this, weak_tracker, l = std::move(l)]() {
          if (!m_slot_new_peers)
            return;

          auto tracker_shared_ptr = weak_tracker.lock();

          if (tracker_shared_ptr)
            receive_new_peers(tracker::Tracker(std::move(tracker_shared_ptr)), const_cast<AddressList*>(&l));
        });
    };

  worker->m_slot_parameters = [this]() {
      // TODO: Lock here!

      TrackerParameters tp;
      tp.numwant = m_numwant;
      tp.uploaded_adjusted = m_info->uploaded_adjusted();
      tp.completed_adjusted = m_info->completed_adjusted();
      tp.download_left = m_info->slot_left()();
      return tp;
    };

  LT_LOG("added tracker : requester:%p group:%u url:%s", worker, itr->group(), itr->url().c_str());

  if (m_slot_tracker_enabled)
    m_slot_tracker_enabled(tracker);

  return itr;
}

// TODO: Use proper flags for insert options.
void
TrackerList::insert_url(unsigned int group, const std::string& url, bool extra_tracker) {
  TrackerWorker* worker{};

  int flags = tracker::TrackerState::flag_enabled;

  if (extra_tracker)
    flags |= tracker::TrackerState::flag_extra_tracker;

  TrackerInfo tracker_info;

  tracker_info.info_hash = m_info->hash();
  tracker_info.obfuscated_hash = m_info->hash_obfuscated();
  tracker_info.local_id = m_info->local_id();
  tracker_info.url = url;
  tracker_info.key = m_key;

  if (std::strncmp("http://", url.c_str(), 7) == 0 ||
      std::strncmp("https://", url.c_str(), 8) == 0) {
    worker = new TrackerHttp(tracker_info, flags);

  } else if (std::strncmp("udp://", url.c_str(), 6) == 0) {
    worker = new TrackerUdp(tracker_info, flags);

  } else if (std::strncmp("dht://", url.c_str(), 6) == 0 && TrackerDht::is_allowed()) {
    worker = new TrackerDht(tracker_info, flags);

  } else {
    LT_LOG("could find matching tracker protocol : url:%s", url.c_str());

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
