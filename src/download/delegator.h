// libTorrent - BitTorrent library
// Copyright (C) 2005-2006, Jari Sundell
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

#include "torrent/data/transfer_list.h"

namespace torrent {

class BitField;
class Block;
class BlockList;
class BlockTransfer;
class Content;
class DownloadMain;
class Piece;
class PeerChunks;
class PeerInfo;
class ChunkSelector;

class Delegator {
public:
  typedef rak::mem_fun1<ChunkSelector, void, uint32_t>              SlotChunkIndex;
  typedef rak::mem_fun2<ChunkSelector, uint32_t, PeerChunks*, bool> SlotChunkFind;
  typedef rak::const_mem_fun1<Content, uint32_t, uint32_t>          SlotChunkSize;

  static const unsigned int block_size = 1 << 14;

  Delegator() : m_aggressive(false) { }

  TransferList*       transfer_list()                     { return &m_transfers; }
  const TransferList* transfer_list() const               { return &m_transfers; }

  BlockTransfer*     delegate(PeerChunks* peerChunks, int affinity);

  bool               get_aggressive()                     { return m_aggressive; }
  void               set_aggressive(bool a)               { m_aggressive = a; }

  void               slot_chunk_find(SlotChunkFind s)     { m_slotChunkFind = s; }
  void               slot_chunk_size(SlotChunkSize s)     { m_slotChunkSize = s; }

  // Don't call this from the outside.
  Block*             delegate_piece(BlockList* c, const PeerInfo* peerInfo);
  Block*             delegate_aggressive(BlockList* c, uint16_t* overlapped, const PeerInfo* peerInfo);

private:
  // Start on a new chunk, returns .end() if none possible. bf is
  // remote peer's bitfield.
  Block*             new_chunk(PeerChunks* pc, bool highPriority);

  Block*             delegate_seeder(PeerChunks* peerChunks);

  TransferList       m_transfers;

  bool               m_aggressive;

  // Propably should add a m_slotChunkStart thing, which will take
  // care of enabling etc, and will be possible to listen to.
  SlotChunkFind      m_slotChunkFind;
  SlotChunkSize      m_slotChunkSize;
};

}

#endif
