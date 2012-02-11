// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#include "config.h"

#include "torrent/exceptions.h"

#include "signal_bitfield.h"

namespace torrent {

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
