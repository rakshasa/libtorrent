#ifndef TORRENT_UTILS_SCHEDULER_H
#define TORRENT_UTILS_SCHEDULER_H

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>
#include <torrent/common.h>

namespace RTORRENT_EXPORT torrent {

namespace utils {

class SchedulerEntry;
class Scheduler;

struct SchedulerHandle {
  SchedulerEntry* entry{};
  Scheduler*      scheduler{};
  std::chrono::microseconds time{};
};

class Scheduler {
public:
  using time_type = std::chrono::microseconds;

  ~Scheduler() = default;

  bool                empty() const                       { return m_heap.empty(); }
  time_type           next_timeout(time_type max_timeout);

  void                erase(SchedulerEntry* entry);

  void                wait_until(SchedulerEntry* entry, time_type time);
  void                wait_for(SchedulerEntry* entry, time_type time);
  void                wait_for_ceil_seconds(SchedulerEntry* entry, time_type time);

  void                update_wait_until(SchedulerEntry* entry, time_type time);
  void                update_wait_for(SchedulerEntry* entry, time_type time);
  void                update_wait_for_ceil_seconds(SchedulerEntry* entry, time_type time);

protected:
  friend class system::Thread;

  void                perform(time_type time);

  void                set_thread_id(std::thread::id id) { m_thread_id = id; }
  void                set_cached_time(time_type t)      { m_cached_time = t; }

private:
  using heap_type = std::vector<std::unique_ptr<SchedulerHandle>>;

  void                push_entry(SchedulerEntry* entry, time_type time);

  std::atomic<std::thread::id> m_thread_id;

  align_cacheline time_type    m_cached_time{};
  heap_type                    m_heap;
};

class SchedulerEntry {
public:
  using slot_type = std::function<void()>;
  using time_type = std::chrono::microseconds;

  SchedulerEntry() = default;
  ~SchedulerEntry();

  bool                is_valid() const     { return m_slot != nullptr; }
  bool                is_scheduled() const { return m_handle != nullptr; }

  slot_type&          slot()               { return m_slot; }
  time_type           time_or_zero() const { return m_handle ? m_handle->time : time_type{}; }

protected:
  friend class Scheduler;

  SchedulerHandle*    handle() const       { return m_handle; }
  void                set_handle(SchedulerHandle* h) { m_handle = h; }

private:
  SchedulerEntry(const SchedulerEntry&) = delete;
  SchedulerEntry& operator=(const SchedulerEntry&) = delete;

  slot_type           m_slot;
  SchedulerHandle*    m_handle{};
};

class ExternalScheduler : public Scheduler {
public:
  void                external_perform(time_type time)           { perform(time); }

  void                external_set_thread_id(std::thread::id id) { set_thread_id(id); }
  void                external_set_cached_time(time_type t)      { set_cached_time(t); }
};

} // namespace torrent::utils

} // namespace torrent

#endif
