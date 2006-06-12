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

class BlockList;
class BlockTransfer;
class PeerInfo;

// If you start adding slots, make sure the rest of the code creates
// copies and clears the original variables before calls to erase etc.

class Block {
public:
  // Using vectors as they will remain small, thus the cost of erase
  // should be small. Later we can do faster erase by ignoring the
  // ordering.
  typedef std::vector<BlockTransfer*> transfer_list;
  typedef uint32_t                    size_type;

  Block() : m_notStalled(0), m_finished(false) { }

  bool                is_stalled() const                           { return m_notStalled == 0; }
  bool                is_finished() const                          { return m_finished; }
  bool                is_transfering() const                       { return !m_transfers.empty(); }

  bool                is_peer_queued(const PeerInfo* p) const      { return find_queued(p) != NULL; }
  bool                is_peer_transfering(const PeerInfo* p) const { return find_transfer(p) != NULL; }

  BlockList*          parent()                                     { return m_parent; }
  const BlockList*    parent() const                               { return m_parent; }
  void                set_parent(BlockList* p)                     { m_parent = p; }

  const Piece&        piece() const                                { return m_piece; }
  void                set_piece(const Piece& p)                    { m_piece = p; }

  uint32_t            index() const                                { return m_piece.index(); }

  size_type           size_all() const                             { return m_queued.size() + m_transfers.size(); }
  size_type           size_not_stalled() const                     { return m_notStalled; }

  void                clear();

  BlockTransfer*      insert(PeerInfo* peerInfo);

  // If the queued or transfering is already removed from the block it
  // will just delete the object. Made static so it can be called when
  // block == NULL.
  void                erase(BlockTransfer* transfer);

  void                transfering(BlockTransfer* transfer);
  void                stalled(BlockTransfer* transfer);

  // Return true if all blocks in the chunk is finished.
  bool                completed(BlockTransfer* transfer);

  const transfer_list* queued() const                              { return &m_queued; }
  const transfer_list* transfers() const                           { return &m_transfers; }

  BlockTransfer*       find(const PeerInfo* p);
  const BlockTransfer* find(const PeerInfo* p) const;

  BlockTransfer*       find_queued(const PeerInfo* p);
  const BlockTransfer* find_queued(const PeerInfo* p) const;

  BlockTransfer*       find_transfer(const PeerInfo* p);
  const BlockTransfer* find_transfer(const PeerInfo* p) const;

private:
//   Block(const Block&);
//   void operator = (const Block&);

  inline void         invalidate_transfer(BlockTransfer* transfer);

  BlockList*          m_parent;
  Piece               m_piece;
  
  uint32_t            m_notStalled;
  bool                m_finished;

  transfer_list       m_queued;
  transfer_list       m_transfers;
};

inline BlockTransfer*
Block::find(const PeerInfo* p) {
  BlockTransfer* transfer;

  if ((transfer = find_queued(p)) != NULL)
    return transfer;
  else
    return find_transfer(p);
}

inline const BlockTransfer*
Block::find(const PeerInfo* p) const {
  const BlockTransfer* transfer;

  if ((transfer = find_queued(p)) != NULL)
    return transfer;
  else
    return find_transfer(p);
}

}

#endif
