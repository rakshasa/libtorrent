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

#ifndef LIBTORRENT_BLOCK_H
#define LIBTORRENT_BLOCK_H

#include <vector>
#include <inttypes.h>
#include <torrent/piece.h>

namespace torrent {

class BlockTransfer;
class PeerInfo;

class Block {
public:

  // Using vectors as they will remain small, thus the cost of erase
  // should be small. Later we can do faster erase by ignoring the
  // ordering.
  typedef std::vector<BlockTransfer*> transfer_list;

  bool                is_queued(const PeerInfo* p) const      { return find_queued(p) != NULL; }
  bool                is_transfering(const PeerInfo* p) const { return find_transfer(p) != NULL; }

  const Piece&        piece() const                           { return m_piece; }
  void                set_piece(const Piece& p)               { m_piece = p; }

  void                clear();

  BlockTransfer*      insert(PeerInfo* peerInfo);

  // If the queued or transfering is already removed from the block it
  // will just delete the object. Made static so it can be called when
  // block == NULL.
  static void         erase(BlockTransfer* transfer);

  static void         transfering(BlockTransfer* transfer);
  static void         stalled(BlockTransfer* transfer);

  static void         completed(BlockTransfer* transfer);

  const transfer_list* queued() const                         { return &m_queued; }
  const transfer_list* transfers() const                      { return &m_transfers; }

  BlockTransfer*       find_queued(const PeerInfo* p);
  const BlockTransfer* find_queued(const PeerInfo* p) const;

  BlockTransfer*       find_transfer(const PeerInfo* p);
  const BlockTransfer* find_transfer(const PeerInfo* p) const;

private:
//   Block(const Block&);
//   void operator = (const Block&);

  inline void         invalidate_transfer(BlockTransfer* transfer);

  Piece               m_piece;

  transfer_list       m_queued;
  transfer_list       m_transfers;
};

}

#endif
