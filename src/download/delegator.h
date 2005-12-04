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
#include <rak/functional.h>

namespace torrent {

class BitField;
class Content;
class DownloadMain;
class DelegatorChunk;
class DelegatorReservee;
class DelegatorPiece;
class Piece;
class PeerChunks;
class ChunkSelector;

class Delegator {
public:
  typedef std::vector<DelegatorChunk*>                             Chunks;
  typedef rak::mem_fn1<ChunkSelector, void, uint32_t>              SlotChunkIndex;
  typedef rak::mem_fn2<ChunkSelector, uint32_t, PeerChunks*, bool> SlotChunkFind;
  typedef rak::mem_fn1<DownloadMain, void, unsigned int>           SlotChunkDone;
  typedef rak::const_mem_fn1<Content, uint32_t, unsigned int>      SlotChunkSize;

  static const unsigned int block_size = 1 << 14;

  Delegator() : m_aggressive(false) { }
  ~Delegator() { clear(); }

  void               clear();

  DelegatorReservee* delegate(PeerChunks* peerChunks, int affinity);

  void               finished(DelegatorReservee& r);

  void               done(unsigned int index);
  void               redo(unsigned int index);

  Chunks&            get_chunks()                         { return m_chunks; }

  bool               get_aggressive()                     { return m_aggressive; }
  void               set_aggressive(bool a)               { m_aggressive = a; }

  void               slot_chunk_enable(SlotChunkIndex s)  { m_slotChunkEnable = s; }
  void               slot_chunk_disable(SlotChunkIndex s) { m_slotChunkDisable = s; }
  void               slot_chunk_find(SlotChunkFind s)     { m_slotChunkFind = s; }
  void               slot_chunk_done(SlotChunkDone s)     { m_slotChunkDone = s; }
  void               slot_chunk_size(SlotChunkSize s)     { m_slotChunkSize = s; }

  // Don't call this from the outside.
  DelegatorPiece*    delegate_piece(DelegatorChunk* c);
  DelegatorPiece*    delegate_aggressive(DelegatorChunk* c, uint16_t* overlapped);

private:
  // Start on a new chunk, returns .end() if none possible. bf is
  // remote peer's bitfield.
  DelegatorPiece*    new_chunk(PeerChunks* pc, bool highPriority);
  DelegatorPiece*    find_piece(const Piece& p);

  bool               all_finished(int index);

  bool               m_aggressive;

  Chunks             m_chunks;

  // Propably should add a m_slotChunkStart thing, which will take
  // care of enabling etc, and will be possible to listen to.
  SlotChunkIndex     m_slotChunkEnable;
  SlotChunkIndex     m_slotChunkDisable;
  SlotChunkFind      m_slotChunkFind;
  SlotChunkDone      m_slotChunkDone;
  SlotChunkSize      m_slotChunkSize;
};

} // namespace torrent

#endif // LIBTORRENT_DELEGATOR_H
