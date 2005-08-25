// libTorrent - BitTorrent library
// Copyright (C) 2005, Jari Sundell
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

#ifndef LIBTORRENT_DELEGATOR_H
#define LIBTORRENT_DELEGATOR_H

#include <string>
#include <vector>
#include <sigc++/signal.h>

#include "delegator_select.h"

namespace torrent {

class BitField;
class DelegatorChunk;
class DelegatorReservee;
class DelegatorPiece;
class Piece;

class Delegator {
public:
  typedef std::vector<DelegatorChunk*>        Chunks;
  typedef sigc::signal1<void, unsigned int>   SignalChunkDone;
  typedef sigc::slot1<uint32_t, unsigned int> SlotChunkSize;

  Delegator() : m_aggressive(false) { }
  ~Delegator() { clear(); }

  void               clear();

  DelegatorReservee* delegate(const BitField& bf, int affinity);

  void               finished(DelegatorReservee& r);

  void               done(unsigned int index);
  void               redo(unsigned int index);

  Chunks&            get_chunks()                         { return m_chunks; }
  DelegatorSelect&   get_select()                         { return m_select; }

  bool               get_aggressive()                     { return m_aggressive; }
  void               set_aggressive(bool a)               { m_aggressive = a; }

  SignalChunkDone&   signal_chunk_done()                  { return m_signalChunkDone; }

  void               slot_chunk_size(SlotChunkSize s)     { m_slotChunkSize = s; }

  // Don't call this from the outside.
  DelegatorPiece*    delegate_piece(DelegatorChunk* c);
  DelegatorPiece*    delegate_aggressive(DelegatorChunk* c, uint16_t* overlapped);

private:
  // Start on a new chunk, returns .end() if none possible. bf is
  // remote peer's bitfield.
  DelegatorPiece*    new_chunk(const BitField& bf, Priority::Type p, int affinity);
  DelegatorPiece*    find_piece(const Piece& p);

  bool               all_finished(int index);

  bool               m_aggressive;

  Chunks             m_chunks;
  DelegatorSelect    m_select;

  SignalChunkDone    m_signalChunkDone;
  SlotChunkSize      m_slotChunkSize;
};

} // namespace torrent

#endif // LIBTORRENT_DELEGATOR_H
