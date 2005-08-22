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

#include "config.h"

#include "torrent/exceptions.h"

#include "chunk_list.h"
#include "storage_chunk.h"

namespace torrent {

bool
ChunkList::has_chunk(size_type index, int prot) const {
  return
    Base::at(index).is_valid() &&
    Base::at(index).chunk()->has_permissions(prot);
}

void
ChunkList::resize(size_type s) {
  if (!empty())
    throw internal_error("ChunkList::resize(...) called on an non-empty object.");

  Base::resize(s);
}

void
ChunkList::clear() {
  if (std::find_if(begin(), end(), std::mem_fun_ref(&ChunkListNode::is_valid)) != end())
    throw internal_error("ChunkList::clear() called but a valid node was found.");

  Base::clear();
}

// Don't use iterator here since we'd like to keep header
// dependencies as low as possible.
void
ChunkList::insert(size_type index, StorageChunk* chunk) {
  ChunkListNode* node = &Base::at(index);

  if (node->is_valid())
    // Explicitly clear here?
    delete node->chunk();

  node->set_chunk(chunk);
  //node->set_index(index);
}

ChunkListNode*
ChunkList::bind(size_type index) {
  ChunkListNode* node = &Base::at(index);
  node->inc_references();

  return node;
}

void
ChunkList::release(ChunkListNode* node) {
  if (node == NULL)
    throw internal_error("ChunkList::release(...) received a node == NULL.");

  if (node->references() <= 0)
    throw internal_error("ChunkList::release(...) received a node with bad reference count.");

  node->dec_references();

  if (node->references() == 0) {
    // Don't delete if we've modified it etc... Consider using a r/w
    // reference count, and remove the write privledge and queue for
    // syncing.

    delete node->chunk();
    node->set_chunk(NULL);
  }
}

}
