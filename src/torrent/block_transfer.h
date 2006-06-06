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

#ifndef LIBTORRENT_BLOCK_TRANSFER_H
#define LIBTORRENT_BLOCK_TRANSFER_H

#include <inttypes.h>
#include <torrent/block.h>

namespace torrent {

class BlockTransfer {
public:
  static const uint32_t position_invalid = ~uint32_t();
  static const uint32_t stall_erased = ~uint32_t();

  BlockTransfer() {}

  bool                is_valid() const              { return m_block; }
  bool                is_erased() const             { return m_stall == stall_erased; }
  bool                is_queued() const             { return m_position == position_invalid; }

  PeerInfo*           peer_info()                   { return m_peerInfo; }
  void                set_peer_info(PeerInfo* p)    { m_peerInfo = p; }

  Block*              block()                       { return m_block; }
  const Block*        block() const                 { return m_block; }
  void                set_block(Block* b)           { m_block = b; }

  uint32_t            position() const              { return m_position; }
  void                set_position(uint32_t p)      { m_position = p; }
  void                adjust_position(uint32_t p)   { m_position += p; }

  uint32_t            stall() const                 { return m_stall; }
  void                set_stall(uint32_t s)         { m_stall = s; }

  void                transfering()                 { m_block->transfering(this); }
  void                completed()                   { m_block->completed(this); }
  void                stalled()                     { if (!is_valid()) return; m_block->stalled(this); }

  void                erase();

private:
  BlockTransfer(const BlockTransfer&);
  void operator = (const BlockTransfer&);

  PeerInfo*           m_peerInfo;
  Block*              m_block;

  uint32_t            m_position;
  uint32_t            m_stall;
};

inline void
BlockTransfer::erase() {
  // Check if the transfer was orphaned, if so just delete it.
  if (!is_valid())
    delete this;
  else
    m_block->erase(this);
}

}

#endif
