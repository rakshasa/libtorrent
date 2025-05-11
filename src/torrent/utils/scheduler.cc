#include "config.h"

#include "torrent/utils/scheduler.h"

#include <algorithm>
#include <cassert>

#include "torrent/exceptions.h"
#include "torrent/utils/chrono.h"

namespace torrent::utils {

SchedulerEntry::~SchedulerEntry() {
  assert(!is_scheduled() && "SchedulerEntry::~SchedulerEntry() called on a scheduled item.");
  assert(m_time == time_type{} && "SchedulerEntry::~SchedulerEntry() called on an item with a time.");

  m_slot = nullptr;
  m_scheduler = nullptr;
  m_time = time_type{};
}


inline void
Scheduler::make_heap() {
  std::make_heap(begin(), end(), [](const SchedulerEntry* a, const SchedulerEntry* b) {
      return a->time() > b->time();
    });
}

inline void
Scheduler::push_heap() {
  std::push_heap(begin(), end(), [](const SchedulerEntry* a, const SchedulerEntry* b) {
      return a->time() > b->time();
    });
}

Scheduler::time_type
Scheduler::next_timeout() const {
  assert(!empty());

  return std::max(front()->time() - m_cached_time, Scheduler::time_type());
}

// We can't make erase/update part of SchedulerItem in case another thread tries to call the
// scheduler, which is not thread-safe.
void
Scheduler::erase(SchedulerEntry* entry) {
  assert(m_thread_id == std::thread::id() || m_thread_id == std::this_thread::get_id());

  if (!entry->is_scheduled())
    return;

  // Check is_valid() after is_schedulerd() so that it is safe to call
  // erase on untouched instances.
  if (!entry->is_valid())
    throw torrent::internal_error("Scheduler::erase(...) called on an invalid entry.");

  if (entry->scheduler() != this)
    throw torrent::internal_error("Scheduler::erase(...) called on an entry that is in another scheduler.");

  auto itr = std::find_if(begin(), end(), [entry](const SchedulerEntry* e) {
      return e == entry;
    });

  if (itr == end())
    throw torrent::internal_error("Scheduler::erase(...) could not find item in queue.");

  entry->set_scheduler(nullptr);
  entry->set_time(Scheduler::time_type{});

  base_type::erase(itr);
  make_heap();
}

void
Scheduler::wait_until(SchedulerEntry* entry, Scheduler::time_type time) {
  assert(m_thread_id == std::thread::id() || m_thread_id == std::this_thread::get_id());

  if (time == Scheduler::time_type())
    throw torrent::internal_error("Scheduler::wait_until(...) received a bad timer.");

  if (time < Scheduler::time_type(365 * 24h))
    throw torrent::internal_error("Scheduler::wait_until(...) received a too small timer.");

  if (!entry->is_valid())
    throw torrent::internal_error("Scheduler::wait_until(...) called on an invalid entry.");

  if (entry->is_scheduled())
    throw torrent::internal_error("Scheduler::wait_until(...) called on an already scheduled entry.");

  entry->set_scheduler(this);
  entry->set_time(time);

  base_type::push_back(entry);
  push_heap();
}

void
Scheduler::wait_for(SchedulerEntry* entry, Scheduler::time_type time) {
  if (time > Scheduler::time_type(10 * 365 * 24h))
    throw torrent::internal_error("Scheduler::wait_after(...) received a too large timer.");

  wait_until(entry, m_cached_time + time);
}

void
Scheduler::wait_for_ceil_seconds(SchedulerEntry* entry, Scheduler::time_type time) {
  if (time > Scheduler::time_type(10 * 365 * 24h))
    throw torrent::internal_error("Scheduler::wait_after_ceil_seconds(...) received a too large timer.");

  wait_until(entry, ceil_seconds(m_cached_time + time));
}

void
Scheduler::update_wait_until(SchedulerEntry* entry, Scheduler::time_type time) {
  assert(m_thread_id == std::thread::id() || m_thread_id == std::this_thread::get_id());

  if (time == Scheduler::time_type())
    throw torrent::internal_error("Scheduler::update_wait(...) received a bad timer.");

  if (time < Scheduler::time_type(365 * 24h))
    throw torrent::internal_error("Scheduler::update_wait(...) received a too small timer.");

  if (!entry->is_valid())
    throw torrent::internal_error("Scheduler::update_wait(...) called on an invalid entry.");

  if (entry->is_scheduled()) {
    if (entry->scheduler() != this)
      throw torrent::internal_error("Scheduler::update_wait(...) called on an entry that is in another scheduler.");

    entry->set_time(time);
    make_heap();
    return;
  }

  entry->set_scheduler(this);
  entry->set_time(time);

  base_type::push_back(entry);
  push_heap();
}

void
Scheduler::update_wait_for(SchedulerEntry* entry, Scheduler::time_type time) {
  if (time > Scheduler::time_type(10 * 365 * 24h))
    throw torrent::internal_error("Scheduler::update_wait_after(...) received a too large timer.");

  update_wait_until(entry, m_cached_time + time);
}

void
Scheduler::update_wait_for_ceil_seconds(SchedulerEntry* entry, Scheduler::time_type time) {
  if (time > Scheduler::time_type(10 * 365 * 24h))
    throw torrent::internal_error("Scheduler::update_wait_after_ceil_seconds(...) received a too large timer.");

  update_wait_until(entry, ceil_seconds(m_cached_time + time));
}

void
Scheduler::perform(Scheduler::time_type current_time) {
  while (!empty() && base_type::front()->time() <= current_time) {
    auto entry = base_type::front();

    std::pop_heap(begin(), end(), [](const SchedulerEntry* a, const SchedulerEntry* b) {
        return a->time() > b->time();
      });
    base_type::pop_back();

    entry->set_scheduler(nullptr);
    entry->set_time(Scheduler::time_type{});
    entry->slot()();
  }
}

} // namespace torrent::utils
