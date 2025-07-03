#ifndef LIBTORRENT_NET_THROTTLE_INTERNAL_H
#define LIBTORRENT_NET_THROTTLE_INTERNAL_H

#include <vector>

#include "torrent/common.h"
#include "torrent/throttle.h"
#include "torrent/utils/scheduler.h"

namespace torrent {

class ThrottleInternal : public Throttle {
public:
  static constexpr int flag_none = 0;
  static constexpr int flag_root = 1;

  ThrottleInternal(int flags);
  ~ThrottleInternal();

  ThrottleInternal*   create_slave();

  bool                is_root() const   { return m_flags & flag_root; }

  void                enable();
  void                disable();

private:
  // Fraction is a fixed-precision value with the given number of bits after the decimal point.
  static constexpr uint32_t fraction_bits = 16;
  static constexpr uint32_t fraction_base = (1 << fraction_bits);

  using SlaveList = std::vector<ThrottleInternal*>;

  void                receive_tick();

  // Distribute quota, return amount of quota used. May be negative
  // if it had more unused quota than is now allowed.
  int32_t             receive_quota(uint32_t quota, uint32_t fraction);

  int                 m_flags;
  SlaveList           m_slave_list;
  SlaveList::iterator m_next_slave;

  uint32_t            m_unused_quota{0};

  std::chrono::microseconds m_time_last_tick;
  utils::SchedulerEntry     m_task_tick;
};

} // namespace torrent

#endif
