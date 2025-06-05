#ifndef TORRENT_UTILS_SCHEDULER_H
#define TORRENT_UTILS_SCHEDULER_H

#include <atomic>
#include <functional>
#include <thread>
#include <vector>
#include <torrent/common.h>

namespace torrent::utils {

class LIBTORRENT_EXPORT Scheduler : public std::vector<SchedulerEntry*> {
public:
  using base_type = std::vector<SchedulerEntry*>;
  using time_type = std::chrono::microseconds;

  using base_type::begin;
  using base_type::end;
  using base_type::front;
  using base_type::size;
  using base_type::empty;
  using base_type::clear;

  ~Scheduler() = default;

  // time_type is microseconds since unix epoch.
  time_type           next_timeout() const;

  void                erase(SchedulerEntry* entry);

  void                wait_until(SchedulerEntry* entry, time_type time);
  void                wait_for(SchedulerEntry* entry, time_type time);
  void                wait_for_ceil_seconds(SchedulerEntry* entry, time_type time);

  void                update_wait_until(SchedulerEntry* entry, time_type time);
  void                update_wait_for(SchedulerEntry* entry, time_type time);
  void                update_wait_for_ceil_seconds(SchedulerEntry* entry, time_type time);

protected:
  friend class Thread;

  void                perform(time_type time);

  void                set_thread_id(std::thread::id id) { m_thread_id = id; }
  void                set_cached_time(time_type t)      { m_cached_time = t; }

private:
  void                make_heap();
  void                push_heap();

  std::atomic<std::thread::id> m_thread_id;
  time_type                    m_cached_time{};
};

class LIBTORRENT_EXPORT SchedulerEntry {
public:
  using slot_type = std::function<void()>;
  using time_type = std::chrono::microseconds;

  SchedulerEntry() = default;
  ~SchedulerEntry();
  bool                is_valid() const     { return m_slot != nullptr; }
  bool                is_scheduled() const { return m_scheduler != nullptr; }

  slot_type&          slot()               { return m_slot; }
  Scheduler*          scheduler() const    { return m_scheduler; }
  time_type           time() const         { return m_time; }

protected:
  friend class Scheduler;

  void                set_scheduler(Scheduler* s) { m_scheduler = s; }
  void                set_time(time_type t)       { m_time = t; }

private:
  SchedulerEntry(const SchedulerEntry&) = delete;
  SchedulerEntry& operator=(const SchedulerEntry&) = delete;

  slot_type           m_slot;
  Scheduler*          m_scheduler{};
  time_type           m_time{};
};

class LIBTORRENT_EXPORT ExternalScheduler : public Scheduler {
public:
  void                external_perform(time_type time)           { perform(time); }

  void                external_set_thread_id(std::thread::id id) { set_thread_id(id); }
  void                external_set_cached_time(time_type t)      { set_cached_time(t); }
};

} // namespace torrent::utils

#endif // TORRENT_UTILS_SCHEDULER_H
