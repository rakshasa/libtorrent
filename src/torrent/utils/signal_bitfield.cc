#include "config.h"

#include "torrent/exceptions.h"

#include "signal_bitfield.h"

namespace torrent {

const unsigned int signal_bitfield::max_size;

// Only the thread owning this signal bitfield should add signals.
unsigned int
signal_bitfield::add_signal(slot_type slot) {
  if (m_size >= max_size)
    throw internal_error("signal_bitfield::add_signal(...): No more available slots.");

  if (!slot)
    throw internal_error("signal_bitfield::add_signal(...): Cannot add empty slot.");

  unsigned int index = m_size;
  __sync_add_and_fetch(&m_size, 1);

  m_slots[index] = slot;
  return index;
}

void
signal_bitfield::work() {
  bitfield_type bitfield;

  while (!__sync_bool_compare_and_swap(&m_bitfield, (bitfield = m_bitfield), 0))
    ; // Do nothing.

  unsigned int i = 0;

  while (bitfield) {
    if ((bitfield & (1 << i))) {
      m_slots[i]();
      bitfield = bitfield & ~(1 << i);
    }

    i++;
  }
}

}
