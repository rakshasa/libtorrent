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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_DELEGATOR_H
#define LIBTORRENT_DELEGATOR_H

#include <string>
#include <vector>
#include <sigc++/signal.h>

#include "utils/bitfield.h"
#include "data/piece.h"
#include "content/delegator_select.h"
#include "content/delegator_chunk.h"

namespace torrent {

class DownloadState;

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

  void               done(int index);
  void               redo(int index);

  Chunks&            get_chunks()                         { return m_chunks; }
  DelegatorSelect&   get_select()                         { return m_select; }

  bool               get_aggressive()                     { return m_aggressive; }
  void               set_aggressive(bool a)               { m_aggressive = a; }

  SignalChunkDone&   signal_chunk_done()                  { return m_signalChunkDone; }

  void               slot_chunk_size(SlotChunkSize s)     { m_slotChunkSize = s; }

private:
  // Start on a new chunk, returns .end() if none possible. bf is
  // remote peer's bitfield.
  DelegatorPiece*    new_chunk(const BitField& bf, Priority::Type p);
  DelegatorPiece*    find_piece(const Piece& p);

  bool               all_finished(int index);

  bool               delegate_piece(DelegatorChunk& c, DelegatorPiece*& p);
  bool               delegate_aggressive(DelegatorChunk& c, DelegatorPiece*& p, uint16_t& overlapped);

  bool               m_aggressive;

  Chunks             m_chunks;
  DelegatorSelect    m_select;

  SignalChunkDone    m_signalChunkDone;
  SlotChunkSize      m_slotChunkSize;
};

} // namespace torrent

#endif // LIBTORRENT_DELEGATOR_H
