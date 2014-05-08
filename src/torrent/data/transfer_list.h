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

#ifndef LIBTORRENT_TRANSFER_LIST_H
#define LIBTORRENT_TRANSFER_LIST_H

#include <vector>
#include <torrent/common.h>
#include lt_tr1_functional

namespace torrent {

class TransferList : public std::vector<BlockList*> {
public:
  typedef std::vector<BlockList*>                    base_type;
  typedef std::vector<std::pair<int64_t, uint32_t> > completed_list_type;

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

  TransferList();
  ~TransferList();

  iterator            find(uint32_t index);
  const_iterator      find(uint32_t index) const;

  const completed_list_type& completed_list() const { return m_completedList; }

  uint32_t            succeeded_count() const { return m_succeededCount; }
  uint32_t            failed_count() const { return m_failedCount; }

  //
  // Internal to libTorrent:
  //

  void                clear();

  iterator            insert(const Piece& piece, uint32_t blockSize);
  iterator            erase(iterator itr);

  void                finished(BlockTransfer* transfer);

  void                hash_succeeded(uint32_t index, Chunk* chunk);
  void                hash_failed(uint32_t index, Chunk* chunk);

  typedef std::function<void (uint32_t)>  slot_chunk_index;
  typedef std::function<void (PeerInfo*)> slot_peer_info;

  slot_chunk_index&   slot_canceled()  { return m_slot_canceled; }
  slot_chunk_index&   slot_completed() { return m_slot_completed; }
  slot_chunk_index&   slot_queued()    { return m_slot_queued; }
  slot_peer_info&     slot_corrupt()   { return m_slot_corrupt; }

private:
  TransferList(const TransferList&);
  void operator = (const TransferList&);

  unsigned int        update_failed(BlockList* blockList, Chunk* chunk);
  void                mark_failed_peers(BlockList* blockList, Chunk* chunk);

  void                retry_most_popular(BlockList* blockList, Chunk* chunk);

  slot_chunk_index    m_slot_canceled;
  slot_chunk_index    m_slot_completed;
  slot_chunk_index    m_slot_queued;
  slot_peer_info      m_slot_corrupt;

  completed_list_type m_completedList;

  uint32_t            m_succeededCount;
  uint32_t            m_failedCount;
};

}

#endif
