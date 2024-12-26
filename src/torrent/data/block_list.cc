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

#include <algorithm>
#include <functional>

#include "block_transfer.h"
#include "block_list.h"
#include "exceptions.h"

namespace torrent {

BlockList::BlockList(const Piece& piece, uint32_t blockLength) :
  m_piece(piece),
  m_priority(PRIORITY_OFF),
  m_finished(0),

  m_failed(0),
  m_attempt(0),

  m_bySeeder(false) {

  if (piece.length() == 0)
    throw internal_error("BlockList::BlockList(...) received zero length piece.");

  // Look into optimizing this by using input iterators in the ctor.
  base_type::resize((m_piece.length() + blockLength - 1) / blockLength);

  // ATM assume offset of 0.
//   uint32_t offset = m_piece.offset();
  uint32_t offset = 0;

  for (iterator itr = begin(), last = end() - 1; itr != last; ++itr, offset += blockLength) {
    itr->set_parent(this);
    itr->set_piece(Piece(m_piece.index(), offset, blockLength));
  }
  
  base_type::back().set_parent(this);
  base_type::back().set_piece(Piece(m_piece.index(), offset, (m_piece.length() % blockLength) ? m_piece.length() % blockLength : blockLength));
}

BlockList::~BlockList() {
  // The default dtor's handles cleaning up the blocks and block transfers.
}

void
BlockList::do_all_failed() {
  clear_finished();
  set_attempt(0);

  // Clear leaders when we want to redownload the chunk.
  std::for_each(begin(), end(), std::mem_fn(&Block::failed_leader));
  std::for_each(begin(), end(), std::mem_fn(&Block::retry_transfer));
}

}
