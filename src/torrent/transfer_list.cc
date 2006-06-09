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

#include "config.h"

#include <algorithm>
#include <rak/functional.h>

#include "block_list.h"
#include "exceptions.h"
#include "piece.h"

#include "transfer_list.h"

namespace torrent {

TransferList::iterator
TransferList::find(uint32_t index) {
  return std::find_if(begin(), end(), rak::equal(index, std::mem_fun(&BlockList::index)));
}

TransferList::const_iterator
TransferList::find(uint32_t index) const {
  return std::find_if(begin(), end(), rak::equal(index, std::mem_fun(&BlockList::index)));
}

void
TransferList::clear() {
  std::for_each(begin(), end(), rak::call_delete<BlockList>());

  base_type::clear();
}

TransferList::iterator
TransferList::insert(const Piece& piece, uint32_t blockSize) {
  if (find(piece.index()) != end())
    throw internal_error("Delegator::new_chunk(...) received an index that is already delegated.");

  BlockList* blockList = new BlockList(piece, blockSize);
  
  return base_type::insert(end(), blockList);
}

TransferList::iterator
TransferList::erase(iterator itr) {
  if (itr == end())
    throw internal_error("TransferList::erase(...) itr == m_chunks.end().");

  delete *itr;

  return base_type::erase(itr);
}

}
