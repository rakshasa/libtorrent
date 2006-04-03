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
#include <functional>
#include <memory>
#include <rak/error_number.h>
#include <rak/functional.h>

#include "torrent/exceptions.h"
#include "torrent/path.h"

#include "chunk.h"
#include "directory.h"
#include "file_meta.h"
#include "entry_list.h"
#include "memory_chunk.h"

namespace torrent {

void
EntryList::push_back(const Path& path, const EntryListNode::Range& range, off_t size) {
  if (sizeof(off_t) != 8)
    throw internal_error("sizeof(off_t) != 8");

  if (size + m_bytesSize < m_bytesSize)
    throw internal_error("Sum of files added to EntryList overflowed 64bit");

  EntryListNode* e = new EntryListNode();

  e->set_position(m_bytesSize);
  e->set_size(size);
  e->set_range(range);

  *e->path() = path;

  base_type::push_back(e);

  m_bytesSize += size;
}

void
EntryList::clear() {
  close();

  std::for_each(begin(), end(), rak::call_delete<EntryListNode>());

  base_type::clear();
  m_bytesSize = 0;
}

void
EntryList::open() {
  if (m_rootDir.empty())
    throw internal_error("EntryList::open() m_rootDir.empty().");

  Path lastPath;

  m_isOpen = true;

  try {
    make_directory(m_rootDir);

    for (iterator itr = begin(), last = end(); itr != last; ++itr) {
      if ((*itr)->file_meta()->is_open())
	throw internal_error("EntryList::open(...) found an already opened file.");
      
      // Do this somewhere else.
      m_slotInsertFileMeta((*itr)->file_meta());
      
      if ((*itr)->path()->empty())
	throw storage_error("Found an empty filename.");

      if (!open_file(&*(*itr), lastPath))
	throw storage_error("Could not open file \"" + m_rootDir + (*itr)->path()->as_string() + "\": " + rak::error_number::current().c_str());
      
      lastPath = *(*itr)->path();
    }

  } catch (storage_error& e) {
    close();
    throw e;
  }
}

void
EntryList::close() {
  if (!m_isOpen)
    return;

  for (iterator itr = begin(), last = end(); itr != last; ++itr) {
    m_slotEraseFileMeta((*itr)->file_meta());
    
    (*itr)->set_completed(0);
  }

  m_isOpen = false;
}

void
EntryList::set_root_dir(const std::string& path) {
  m_rootDir = path;

  for (iterator itr = begin(), last = end(); itr != last; ++itr) {
    if ((*itr)->file_meta()->is_open())
      throw internal_error("EntryList::set_root_dir(...) found an already opened file.");
    
    (*itr)->file_meta()->set_path(m_rootDir + (*itr)->path()->as_string());
  }
}

bool
EntryList::resize_all() {
  iterator itr = std::find_if(begin(), end(), std::not1(std::mem_fun(&EntryListNode::resize_file)));

  if (itr != end()) {
    std::for_each(++itr, end(), std::mem_fun(&EntryListNode::resize_file));
    return false;
  }

  return true;
}
					   
EntryList::iterator
EntryList::at_position(iterator itr, off_t offset) {
  while (itr != end())
    if (offset >= (*itr)->position() + (*itr)->size())
      ++itr;
    else
      return itr;

  return itr;
}

bool
EntryList::open_file(EntryListNode* node, const Path& lastPath) {
  make_directory(m_rootDir, node->path()->begin(), --node->path()->end(), lastPath.begin(), lastPath.end());

  // Some torrents indicate an empty directory by having a path with
  // an empty last element. This entry must be zero length.
  if (node->path()->back().empty())
    return node->size() == 0;

  return
    node->file_meta()->prepare(MemoryChunk::prot_read | MemoryChunk::prot_write, SocketFile::o_create) ||
    node->file_meta()->prepare(MemoryChunk::prot_read, SocketFile::o_create);
}

inline MemoryChunk
EntryList::create_chunk_part(iterator itr, off_t offset, uint32_t length, int prot) {
  MemoryChunk chunk;

  offset -= (*itr)->position();
  length = std::min<off_t>(length, (*itr)->size() - offset);

  if (offset < 0)
    throw internal_error("EntryList::chunk_part(...) caught a negative offset");

  if ((*itr)->file_meta()->prepare(prot))
    chunk = (*itr)->file_meta()->get_file().create_chunk(offset, length, prot, MemoryChunk::map_shared);
  else
    chunk.clear();

  return chunk;
}

Chunk*
EntryList::create_chunk(off_t offset, uint32_t length, int prot) {
  if (offset + length > m_bytesSize)
    throw internal_error("Tried to access chunk out of range in EntryList");

  std::auto_ptr<Chunk> chunk(new Chunk);

  for (iterator itr = std::find_if(begin(), end(), std::bind2nd(std::mem_fun(&EntryListNode::is_valid_position), offset)); length != 0; ++itr) {

    if (itr == end())
      throw internal_error("EntryList could not find a valid file for chunk");

    if ((*itr)->size() == 0)
      continue;

    MemoryChunk mc = create_chunk_part(itr, offset, length, prot);

    if (!mc.is_valid())
      return NULL;

    chunk->push_back(mc);
    offset += mc.size();
    length -= mc.size();
  }

  return !chunk->empty() ? chunk.release() : NULL;
}

}
