#include "config.h"

#include "net/throttle_internal.h"

#include <algorithm>

#include "net/throttle_list.h"
#include "torrent/exceptions.h"

namespace torrent {

// Plans:
//
// Make ThrottleList do a callback when it needs more? This would
// allow us to remove us from the task scheduler when we're full. Also
// this would let us be abit more flexible with the interval.

ThrottleInternal::ThrottleInternal(int flags) :
    m_flags(flags),
    m_next_slave(m_slave_list.end()),
    m_time_last_tick(torrent::this_thread::cached_time()) {

  if (is_root())
    m_task_tick.slot() = [this] { receive_tick(); };
}

ThrottleInternal::~ThrottleInternal() {
  if (is_root())
    torrent::this_thread::scheduler()->erase(&m_task_tick);

  for (const auto& t : m_slave_list)
    delete t;
}

void
ThrottleInternal::enable() {
  m_throttleList->enable();
  for (auto t : m_slave_list)
    t->m_throttleList->enable();

  if (is_root()) {
    // We need to start the ticks, and make sure we set timeLastTick
    // to a value that gives an reasonable initial quota.
    m_time_last_tick = std::max(torrent::this_thread::cached_time() - 1s, 0us);

    receive_tick();
  }
}

void
ThrottleInternal::disable() {
  for (auto t : m_slave_list)
    t->m_throttleList->disable();
  m_throttleList->disable();

  if (is_root())
    torrent::this_thread::scheduler()->erase(&m_task_tick);
}

ThrottleInternal*
ThrottleInternal::create_slave() {
  auto slave = new ThrottleInternal(flag_none);

  slave->m_maxRate = m_maxRate;
  slave->m_throttleList = new ThrottleList();

  if (m_throttleList->is_enabled())
    slave->enable();

  m_slave_list.push_back(slave);
  m_next_slave = m_slave_list.end();

  return slave;
}

void
ThrottleInternal::receive_tick() {
  if (torrent::this_thread::cached_time() < m_time_last_tick + 90ms)
    throw internal_error("ThrottleInternal::receive_tick() called at a to short interval.");

  uint64_t count_usec = (torrent::this_thread::cached_time() - m_ptr()->m_time_last_tick).count();

  uint32_t quota    = count_usec * m_maxRate / 1000000;
  uint32_t fraction = count_usec * fraction_base / 1000000;

  receive_quota(quota, fraction);

  torrent::this_thread::scheduler()->wait_for_ceil_seconds(&m_task_tick, std::chrono::microseconds(calculate_interval()));
  m_time_last_tick = torrent::this_thread::cached_time();
}

int32_t
ThrottleInternal::receive_quota(uint32_t quota, uint32_t fraction) {
  m_unused_quota += quota;

  while (m_next_slave != m_slave_list.end()) {
    // TODO: Add logging and see if the use of 'quota' below is causing the quota to be
    // over-allocated to the next slave, instead of a smoother distribution.
    //
    // E.g. we might want to spread it over the next few slaves, especially if we have a very large
    // number.

    uint32_t need = std::min<uint32_t>(quota, static_cast<uint64_t>(fraction) * (*m_next_slave)->max_rate() >> fraction_bits);

    if (need > m_unused_quota)
      break;

    m_unused_quota -= (*m_next_slave)->receive_quota(need, fraction);
    m_throttleList->add_rate((*m_next_slave)->throttle_list()->rate_added());

    ++m_next_slave;
  }

  uint32_t need = std::min<uint32_t>(quota, static_cast<uint64_t>(fraction) * m_maxRate >> fraction_bits);

  if (m_next_slave == m_slave_list.end() && need <= m_unused_quota) {
    m_unused_quota -= m_throttleList->update_quota(need);
    m_next_slave = m_slave_list.begin();
  }

  // Return how much quota we used, but keep as much as one allocation's worth until the next tick
  // to avoid rounding errors.
  int32_t used = quota;

  if (quota < m_unused_quota) {
    used -= m_unused_quota - quota;
    m_unused_quota = quota;
  }

  // Amount used may be negative if rate changed between last tick and now.
  return used;
}

} // namespace torrent
