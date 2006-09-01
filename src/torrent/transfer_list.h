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

#ifndef LIBTORRENT_TRANSFER_LIST_H
#define LIBTORRENT_TRANSFER_LIST_H

#include <functional>
#include <vector>
#include <inttypes.h>
#include <torrent/common.h>

namespace torrent {

class BlockList;
class BlockTransfer;
class Chunk;
class ChunkSelector;
class DownloadMain;
class PeerInfo;
class Piece;

class TransferList : public std::vector<BlockList*> {
public:
  typedef std::vector<BlockList*> base_type;

  using base_type::value_type;
  using base_type::reference;
  using base_type::difference_type;

  using base_type::iterator;
  using base_type::const_iterator;
  using base_type::reverse_iterator;
  using base_type::const_reverse_iterator;
  using base_type::size;
  using base_type::empty;

  using base_type::begin;
  using base_type::end;
  using base_type::rbegin;
  using base_type::rend;

  TransferList() :
    m_slotCanceled(slot_canceled_type(slot_canceled_op(NULL), NULL)),
    m_slotCompleted(slot_completed_type(slot_completed_op(NULL), NULL)),
    m_slotQueued(slot_queued_type(slot_queued_op(NULL), NULL)),
    m_slotCorrupt(slot_corrupt_type(slot_corrupt_op(NULL), NULL)) { }

  iterator            find(uint32_t index);
  const_iterator      find(uint32_t index) const;

  // Internal to libTorrent:

  void                clear();

  iterator            insert(const Piece& piece, uint32_t blockSize);
  iterator            erase(iterator itr);

  void                finished(BlockTransfer* transfer);

  void                hash_succeded(uint32_t index);
  void                hash_failed(uint32_t index, Chunk* chunk);

  typedef std::mem_fun1_t<void, ChunkSelector, uint32_t> slot_canceled_op;
  typedef std::binder1st<slot_canceled_op>               slot_canceled_type;

  typedef std::mem_fun1_t<void, DownloadMain, uint32_t>  slot_completed_op;
  typedef std::binder1st<slot_completed_op>              slot_completed_type;

  typedef std::mem_fun1_t<void, ChunkSelector, uint32_t> slot_queued_op;
  typedef std::binder1st<slot_queued_op>                 slot_queued_type;

  typedef std::mem_fun1_t<void, DownloadMain, PeerInfo*> slot_corrupt_op;
  typedef std::binder1st<slot_corrupt_op>                slot_corrupt_type;

  void                slot_canceled(slot_canceled_type s)   { m_slotCanceled = s; }
  void                slot_completed(slot_completed_type s) { m_slotCompleted = s; }
  void                slot_queued(slot_queued_type s)       { m_slotQueued = s; }

  void                slot_corrupt(slot_corrupt_type s)     { m_slotCorrupt = s; }

private:
  TransferList(const TransferList&);
  void operator = (const TransferList&);

  unsigned int        update_failed(BlockList* blockList, Chunk* chunk);
  void                mark_failed_peers(BlockList* blockList);

  void                retry_most_popular(BlockList* blockList, Chunk* chunk);

  slot_canceled_type  m_slotCanceled;
  slot_completed_type m_slotCompleted;
  slot_queued_type    m_slotQueued;
  slot_corrupt_type   m_slotCorrupt;
};

}

#endif
