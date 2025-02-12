#include "config.h"

#include <functional>
#include <random>

#include "net/address_list.h"
#include "torrent/exceptions.h"
#include "torrent/download_info.h"
#include "torrent/tracker.h"
#include "torrent/tracker_list.h"
#include "torrent/utils/log.h"
#include "torrent/utils/option_strings.h"
#include "tracker/tracker_dht.h"
#include "tracker/tracker_http.h"
#include "tracker/tracker_udp.h"

#include "globals.h"

#define LT_LOG_TRACKER(log_level, log_fmt, ...)                         \
  lt_log_print_info(LOG_TRACKER_##log_level, info(), "tracker_list", log_fmt, __VA_ARGS__);

namespace torrent {

TrackerList::TrackerList() :
  m_info(NULL),
  m_state(DownloadInfo::STOPPED),

  m_key(0),
  m_numwant(-1) {
}

bool
TrackerList::has_active() const {
  return std::any_of(begin(), end(), std::mem_fn(&Tracker::is_busy));
}

bool
TrackerList::has_active_not_scrape() const {
  return std::any_of(begin(), end(), std::mem_fn(&Tracker::is_busy_not_scrape));
}

bool
TrackerList::has_active_in_group(uint32_t group) const {
  return std::any_of(begin_group(group), end_group(group), std::mem_fn(&Tracker::is_busy));
}

bool
TrackerList::has_active_not_scrape_in_group(uint32_t group) const {
  return std::any_of(begin_group(group), end_group(group), std::mem_fn(&Tracker::is_busy_not_scrape));
}

bool
TrackerList::has_usable() const {
  return std::any_of(begin(), end(), std::mem_fn(&Tracker::is_usable));
}

unsigned int
TrackerList::count_active() const {
  return std::count_if(begin(), end(), std::mem_fn(&Tracker::is_busy));
}

unsigned int
TrackerList::count_usable() const {
  return std::count_if(begin(), end(), std::mem_fn(&Tracker::is_usable));
}

void
TrackerList::close_all_excluding(int event_bitmap) {
  for (auto tracker : *this) {
    if ((event_bitmap & (1 << tracker->state().latest_event())))
      continue;

    tracker->close();
  }
}

void
TrackerList::disown_all_including(int event_bitmap) {
  for (auto& tracker : *this) {
    if ((event_bitmap & (1 << tracker->state().latest_event())))
      tracker->disown();
  }
}

void
TrackerList::clear() {
  for (const auto& tracker : *this) {
    delete tracker;
  }
  base_type::clear();
}

void
TrackerList::clear_stats() {
  std::for_each(begin(), end(), std::mem_fn(&Tracker::clear_stats));
}

void
TrackerList::send_event(Tracker* tracker, TrackerState::event_enum new_event) {
  if (!tracker->is_usable() || new_event == TrackerState::EVENT_SCRAPE)
    return;

  if (tracker->is_busy()) {
    if (tracker->state().latest_event() != TrackerState::EVENT_SCRAPE)
      return;

    tracker->close();
  }

  tracker->send_event(new_event);
  tracker->inc_request_counter();

  LT_LOG_TRACKER(INFO, "sending '%s (group:%u url:%s)",
                 option_as_string(OPTION_TRACKER_EVENT, new_event),
                 tracker->group(), tracker->url().c_str());
}

void
TrackerList::send_scrape(Tracker* tracker) {
  if (tracker->is_busy() || !tracker->is_usable())
    return;

  if (!(tracker->flags() & Tracker::flag_can_scrape))
    return;

  if (rak::timer::from_seconds(tracker->state().scrape_time_last()) + rak::timer::from_seconds(10 * 60) > cachedTime )
    return;

  tracker->send_scrape();
  tracker->inc_request_counter();

  LT_LOG_TRACKER(INFO, "sending scrape (group:%u url:%s)",
                 tracker->group(), tracker->url().c_str());
}

TrackerList::iterator
TrackerList::insert(unsigned int group, Tracker* tracker) {
  tracker->set_group(group);

  iterator itr = base_type::insert(end_group(group), tracker);

  if (m_slot_tracker_enabled)
    m_slot_tracker_enabled(tracker);

  return itr;
}

// TODO: Use proper flags for insert options.
void
TrackerList::insert_url(unsigned int group, const std::string& url, bool extra_tracker) {
  Tracker* tracker;
  int flags = Tracker::flag_enabled;

  if (extra_tracker)
    flags |= Tracker::flag_extra_tracker;

  if (std::strncmp("http://", url.c_str(), 7) == 0 ||
      std::strncmp("https://", url.c_str(), 8) == 0) {
    tracker = new TrackerHttp(this, url, flags);

  } else if (std::strncmp("udp://", url.c_str(), 6) == 0) {
    tracker = new TrackerUdp(this, url, flags);

  } else if (std::strncmp("dht://", url.c_str(), 6) == 0 && TrackerDht::is_allowed()) {
    tracker = new TrackerDht(this, url, flags);

  } else {
    LT_LOG_TRACKER(WARN, "could find matching tracker protocol (url:%s)", url.c_str());

    if (extra_tracker)
      throw torrent::input_error("could find matching tracker protocol (url:" + url + ")");

    return;
  }

  LT_LOG_TRACKER(INFO, "added tracker (group:%i url:%s)", group, url.c_str());
  insert(group, tracker);
}

TrackerList::iterator
TrackerList::find_url(const std::string& url) {
  return std::find_if(begin(), end(), [&url](auto tracker) { return tracker->url() == url; });
}

TrackerList::iterator
TrackerList::find_usable(iterator itr) {
  return std::find_if(itr, end(), std::mem_fn(&Tracker::is_usable));
}

TrackerList::const_iterator
TrackerList::find_usable(const_iterator itr) const {
  return std::find_if(itr, end(), std::mem_fn(&Tracker::is_usable));
}

TrackerList::iterator
TrackerList::find_next_to_request(iterator itr) {
  auto preferred = itr = std::find_if(itr, end(), std::mem_fn(&Tracker::can_request_state));

  if (preferred == end())
    return end();

  auto preferred_state = (*preferred)->state();

  if (preferred_state.failed_counter() == 0)
    return preferred;

  while (++itr != end()) {
    if (!(*itr)->can_request_state())
      continue;

    auto itr_state = (*itr)->state();

    if (itr_state.failed_counter() != 0) {
      if (itr_state.failed_time_next() < preferred_state.failed_time_next()) {
        preferred = itr;
        preferred_state = (*preferred)->state();
      }

    } else {
      if (itr_state.success_time_next() < preferred_state.failed_time_next()) {
        preferred = itr;
        preferred_state = (*preferred)->state();
      }

      break;
    }
  }

  return preferred;
}

TrackerList::iterator
TrackerList::begin_group(unsigned int group) {
  return std::find_if(begin(), end(), [group](Tracker* t) { return group <= t->group(); });
}

TrackerList::const_iterator
TrackerList::begin_group(unsigned int group) const {
  return std::find_if(begin(), end(), [group](Tracker* t) { return group <= t->group(); });
}

TrackerList::size_type
TrackerList::size_group() const {
  return !empty() ? back()->group() + 1 : 0;
}

void
TrackerList::cycle_group(unsigned int group) {
  iterator itr = begin_group(group);
  iterator prev = itr;

  if (itr == end() || (*itr)->group() != group)
    return;

  while (++itr != end() && (*itr)->group() == group) {
    std::iter_swap(itr, prev);
    prev = itr;
  }
}

TrackerList::iterator
TrackerList::promote(iterator itr) {
  iterator first = begin_group((*itr)->group());

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
    iterator tmp = end_group((*itr)->group());
    std::shuffle(itr, tmp, rng);

    itr = tmp;
  }
}

void
TrackerList::receive_success(Tracker* tracker, AddressList* l) {
  iterator itr = find(tracker);

  if (itr == end() || tracker->is_busy())
    throw internal_error("TrackerList::receive_success(...) called but the iterator is invalid.");

  // Promote the tracker to the front of the group since it was
  // successfull.
  promote(itr);

  l->sort();
  l->erase(std::unique(l->begin(), l->end()), l->end());

  LT_LOG_TRACKER(INFO, "received %u peers (url:%s)", l->size(), tracker->url().c_str());

  auto tracker_state = tracker->state();

  tracker_state.m_success_time_last = cachedTime.seconds();
  tracker_state.m_success_counter++;
  tracker_state.m_failed_counter = 0;
  tracker_state.m_latest_sum_peers = l->size();
  tracker->set_state(tracker_state);

  auto new_peers = m_slot_success(tracker, l);

  tracker_state = tracker->state();
  tracker_state.m_latest_new_peers = new_peers;
  tracker->set_state(tracker_state);
}

void
TrackerList::receive_failed(Tracker* tracker, const std::string& msg) {
  iterator itr = find(tracker);

  if (itr == end() || tracker->is_busy())
    throw internal_error("TrackerList::receive_failed(...) called but the iterator is invalid.");

  LT_LOG_TRACKER(INFO, "failed to send request to tracker (url:%s msg:%s)", tracker->url().c_str(), msg.c_str());

  auto tracker_state = tracker->state();

  tracker_state.m_failed_time_last = cachedTime.seconds();
  tracker_state.m_failed_counter++;
  tracker->set_state(tracker_state);

  m_slot_failed(tracker, msg);
}

void
TrackerList::receive_scrape_success(Tracker* tracker) {
  iterator itr = find(tracker);

  if (itr == end() || tracker->is_busy())
    throw internal_error("TrackerList::receive_success(...) called but the iterator is invalid.");

  LT_LOG_TRACKER(INFO, "received scrape from tracker (url:%s)", tracker->url().c_str());

  auto tracker_state = tracker->state();

  tracker_state.m_scrape_time_last = cachedTime.seconds();
  tracker_state.m_scrape_counter++;
  tracker->set_state(tracker_state);

  if (m_slot_scrape_success)
    m_slot_scrape_success(tracker);
}

void
TrackerList::receive_scrape_failed(Tracker* tracker, const std::string& msg) {
  iterator itr = find(tracker);

  if (itr == end() || tracker->is_busy())
    throw internal_error("TrackerList::receive_failed(...) called but the iterator is invalid.");

  LT_LOG_TRACKER(INFO, "failed to send scrape to tracker (url:%s msg:%s)", tracker->url().c_str(), msg.c_str());

  if (m_slot_scrape_failed)
    m_slot_scrape_failed(tracker, msg);
}

}
