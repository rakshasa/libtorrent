#include "config.h"

#include "signal_bitfield.h"

#include <cassert>
#include "torrent/exceptions.h"

namespace torrent {

const unsigned int signal_bitfield::max_size;

// Only the thread owning this signal bitfield should add signals.
unsigned int
signal_bitfield::add_signal(slot_type slot) {
  if (m_thread_id != std::this_thread::get_id())
    throw internal_error("signal_bitfield::add_signal(...): Only the owning thread can add signals.");

  if (m_size >= max_size)
    throw internal_error("signal_bitfield::add_signal(...): No more available slots.");

  if (!slot)
    throw internal_error("signal_bitfield::add_signal(...): Cannot add empty slot.");

  unsigned int index = m_size++;
  m_slots[index] = slot;

  return index;
}

void
signal_bitfield::work() {
  // static_assert(std::atomic<bitfield_type>()::is_always_lock_free(), "signal_bitfield::work(...): Bitfield type is not lockfree.");

  if (m_thread_id != std::this_thread::get_id())
    throw internal_error("signal_bitfield::work(...): Only the owning thread can do work for signal bitfields.");

  auto bitfield = m_bitfield.exchange(0);

  for (unsigned int i = 0; bitfield != 0 && i < m_size; i++) {
    if ((bitfield & (1 << i))) {
      m_slots[i]();
      bitfield = bitfield & ~(1 << i);
    }
  }
}

}
