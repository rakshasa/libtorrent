#include "config.h"

#include "test/helpers/tracker_test.h"

#include "net/address_list.h"
#include "test/torrent/test_tracker_list.h"

#include <cppunit/extensions/HelperMacros.h>

// TrackerTest does not fully support threaded tracker yet, so review the code if such a requirement
// is needed.

uint32_t return_new_peers = 0xdeadbeef;

torrent::tracker::Tracker
TrackerTest::new_tracker([[maybe_unused]] torrent::TrackerList* parent, const std::string& url, int flags) {
  auto tracker_info = torrent::TrackerInfo{
    // .info_hash = m_info->hash(),
    // .obfuscated_hash = m_info->hash_obfuscated(),
    // .local_id = m_info->local_id(),
    // .key = m_key
  };
  tracker_info.url = url;

  return torrent::tracker::Tracker(std::make_shared<TrackerTest>(std::move(tracker_info), flags));
}

void
TrackerTest::insert_tracker(torrent::TrackerList* parent, int group, torrent::tracker::Tracker tracker) {
  // Insert into partent then override slots.

  parent->insert(group, tracker);

  tracker.get_worker()->m_slot_enabled = [parent, tracker]() {
      if (parent->slot_tracker_enabled())
        parent->slot_tracker_enabled()(tracker);
    };
  tracker.get_worker()->m_slot_disabled = [parent, tracker]() {
      if (parent->slot_tracker_disabled())
        parent->slot_tracker_disabled()(tracker);
    };
  tracker.get_worker()->m_slot_success = [parent, tracker](torrent::AddressList&& l) {
      auto t = tracker;
      parent->receive_success(std::move(t), &l);
    };
  tracker.get_worker()->m_slot_failure = [parent, tracker](const std::string& msg) {
      auto t = tracker;
      parent->receive_failed(std::move(t), msg);
    };
  tracker.get_worker()->m_slot_scrape_success = [parent, tracker]() {
      auto t = tracker;
      parent->receive_scrape_success(std::move(t));
    };
  tracker.get_worker()->m_slot_scrape_failure = [parent, tracker](const std::string& msg) {
      auto t = tracker;
      parent->receive_scrape_failed(std::move(t), msg);
    };
}

void
TrackerTest::set_success(uint32_t counter, uint32_t time_last) {
  auto guard = lock_guard();
  state().m_success_counter = counter;
  state().m_success_time_last = time_last;
  state().set_normal_interval(torrent::tracker::TrackerState::default_normal_interval);
  state().set_min_interval(torrent::tracker::TrackerState::default_min_interval);
}

void
TrackerTest::set_failed(uint32_t counter, uint32_t time_last) {
  auto guard = lock_guard();
  state().m_failed_counter = counter;
  state().m_failed_time_last = time_last;
  state().m_normal_interval = 0;
  state().m_min_interval = 0;
}

void
TrackerTest::set_latest_new_peers(uint32_t peers) {
  auto guard = lock_guard();
  state().m_latest_new_peers = peers;
}

void
TrackerTest::set_latest_sum_peers(uint32_t peers) {
  auto guard = lock_guard();
  state().m_latest_sum_peers = peers;
}

void
TrackerTest::set_new_normal_interval(uint32_t timeout) {
  auto guard = lock_guard();
  state().set_normal_interval(timeout);
}

void
TrackerTest::set_new_min_interval(uint32_t timeout) {
  auto guard = lock_guard();
  state().set_min_interval(timeout);
}

void
TrackerTest::send_event(torrent::tracker::TrackerState::event_enum new_state) {
  // Trackers close on-going requests when new state is sent.
  m_busy = true;
  m_open = true;
  m_requesting_state = new_state;
  lock_and_set_latest_event(new_state);
}

void
TrackerTest::send_scrape() {
  // We ignore scrapes if we're already making a request.
  // if (m_open)
  //   return;

  m_busy = true;
  m_open = true;
  m_requesting_state = torrent::tracker::TrackerState::EVENT_SCRAPE;

  lock_and_set_latest_event(torrent::tracker::TrackerState::EVENT_SCRAPE);
}

bool
TrackerTest::trigger_success(uint32_t new_peers, uint32_t sum_peers) {
  CPPUNIT_ASSERT(is_busy() && "TrackerTest::trigger_success: is_busy()");
  CPPUNIT_ASSERT(is_open() && "TrackerTest::trigger_success: is_open()");

  torrent::AddressList address_list;

  for (unsigned int i = 0; i != sum_peers; i++) {
    torrent::sa_inet_union sa{};
    sa.inet.sin_family = AF_INET;
    sa.inet.sin_port = htons(0x100 + i);

    address_list.push_back(sa);
  }

  address_list.sort_and_unique();

  return trigger_success(&address_list, new_peers);
}

bool
TrackerTest::trigger_success(torrent::AddressList* address_list, uint32_t new_peers) {
  CPPUNIT_ASSERT(is_busy() && "TrackerTest::trigger_success: is_busy()");
  CPPUNIT_ASSERT(is_open() && "TrackerTest::trigger_success: is_open()");
  CPPUNIT_ASSERT(address_list != nullptr && "TrackerTest::trigger_success: address_list == nullptr");

  m_busy = false;
  m_open = !(state().flags() & flag_close_on_done);
  return_new_peers = new_peers;

  if (state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE) {
    if (m_slot_scrape_success)
      m_slot_scrape_success();

  } else {
    {
      auto guard = lock_guard();
      state().set_normal_interval(torrent::tracker::TrackerState::default_normal_interval);
      state().set_min_interval(torrent::tracker::TrackerState::default_min_interval);
    }

    if (m_slot_success)
      m_slot_success(std::move(*address_list));
  }

  m_requesting_state = -1;
  return true;
}

bool
TrackerTest::trigger_failure() {
  CPPUNIT_ASSERT(is_busy() && "TrackerTest::trigger_failure: is_busy()");
  CPPUNIT_ASSERT(is_open() && "TrackerTest::trigger_failure: is_open()");

  m_busy = false;
  m_open = !(state().flags() & flag_close_on_done);
  return_new_peers = 0;

  if (state().latest_event() == torrent::tracker::TrackerState::EVENT_SCRAPE) {
    if (m_slot_scrape_failure)
      m_slot_scrape_failure("failed");

  } else {
    {
      auto guard = lock_guard();
      state().set_normal_interval(0);
      state().set_min_interval(0);
    }

    if (m_slot_failure)
      m_slot_failure("failed");
  }

  m_requesting_state = -1;
  return true;
}

bool
TrackerTest::trigger_scrape() {
  if (!is_busy() || !is_open())
    return false;

  if (state().latest_event() != torrent::tracker::TrackerState::EVENT_SCRAPE)
    return false;

  return trigger_success();
}

int
TrackerTest::count_active(torrent::TrackerList* parent) {
  return std::count_if(parent->begin(), parent->end(), std::mem_fn(&torrent::tracker::Tracker::is_busy));
}

int
TrackerTest::count_usable(torrent::TrackerList* parent) {
  return std::count_if(parent->begin(), parent->end(), std::mem_fn(&torrent::tracker::Tracker::is_usable));
}
