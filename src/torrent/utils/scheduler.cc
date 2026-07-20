#include "config.h"

#include "torrent/utils/scheduler.h"

#include <algorithm>
#include <cassert>

#include "torrent/exceptions.h"
#include "torrent/utils/chrono.h"

namespace torrent::utils {

static constexpr auto compare = [](const std::unique_ptr<SchedulerHandle>& a,
                                   const std::unique_ptr<SchedulerHandle>& b) {
  return a->time > b->time;
};

SchedulerEntry::~SchedulerEntry() {
  assert(!is_scheduled() && "SchedulerEntry::~SchedulerEntry() called on a scheduled item.");

  m_slot = nullptr;
}

Scheduler::time_type
Scheduler::next_timeout(Scheduler::time_type max_timeout) {
  while (!m_heap.empty() && m_heap.front()->entry == nullptr) {
    std::ranges::pop_heap(m_heap, compare);
    m_heap.pop_back();
  }

  if (m_heap.empty())
    return max_timeout;

  auto timeout = m_heap.front()->time - m_cached_time;

  if (timeout >= max_timeout)
    return max_timeout;

  return std::max(timeout, Scheduler::time_type());
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

  if (entry->m_handle->scheduler != this)
    throw torrent::internal_error("Scheduler::erase(...) called on an entry that is in another scheduler.");

  entry->m_handle->entry = nullptr;
  entry->set_handle(nullptr);
}

void
Scheduler::push_entry(SchedulerEntry* entry, time_type time) {
  auto handle = std::make_unique<SchedulerHandle>(SchedulerHandle{entry, this, time});
  entry->set_handle(handle.get());

  m_heap.push_back(std::move(handle));
  std::ranges::push_heap(m_heap, compare);
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

  push_entry(entry, time);
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
    if (entry->m_handle->scheduler != this)
      throw torrent::internal_error("Scheduler::update_wait(...) called on an entry that is in another scheduler.");

    entry->m_handle->entry = nullptr;
    push_entry(entry, time);
    return;
  }

  push_entry(entry, time);
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
  while (!m_heap.empty() && m_heap.front()->time <= current_time) {
    std::ranges::pop_heap(m_heap, compare);
    auto handle = std::move(m_heap.back());
    m_heap.pop_back();

    if (handle->entry == nullptr)
      continue;

    handle->entry->set_handle(nullptr);
    handle->entry->slot()();
  }
}

} // namespace torrent::utils
