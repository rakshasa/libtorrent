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

#include <algorithm>
#include <functional>
#include <memory>

#include "torrent/exceptions.h"

#include "chunk.h"
#include "file_meta.h"
#include "entry_list.h"
#include "memory_chunk.h"
#include "path.h"

namespace torrent {

void
EntryList::push_back(const Path& path, const EntryListNode::Range& range, off_t size) {
  if (sizeof(off_t) != 8)
    throw internal_error("sizeof(off_t) != 8");

  if (size + m_bytesSize < m_bytesSize)
    throw internal_error("Sum of files added to EntryList overflowed 64bit");

  Base::push_back(EntryListNode());
  back().set_position(m_bytesSize);
  back().set_size(size);
  back().set_range(range);

  *back().path() = path;

  m_bytesSize += size;
}

void
EntryList::clear() {
  close();

  Base::clear();
  m_bytesSize = 0;
}

void
EntryList::open(const std::string& root) {
  Path::mkdir(root);
  Path lastPath;

  for (iterator itr = begin(), last = end(); itr != last; ++itr) {
    if (itr->file_meta() != NULL)
      throw internal_error("EntryList::open(...) found an already opened file.");
      
    itr->set_file_meta(m_slotInsertFileMeta(root + itr->path()->as_string()));
      
    if (!open_file(root, itr->file_meta(), *itr->path(), lastPath)) {
      close();
      throw storage_error("Could no open file \"" + root + itr->path()->as_string() + "\"");
    }
      
    lastPath = *itr->path();
  }
}

void
EntryList::close() {
  for (iterator itr = begin(), last = end(); itr != last; ++itr)
    if (itr->is_valid()) {
      m_slotEraseFileMeta(itr->file_meta());

      itr->set_file_meta(NULL);
      itr->set_completed(0);
    }
}

void
EntryList::sync_all() {
  std::for_each(begin(), end(), std::mem_fun_ref(&EntryListNode::sync_file));
}

bool
EntryList::resize_all() {
  iterator itr = std::find_if(begin(), end(), std::not1(std::mem_fun_ref(&EntryListNode::resize_file)));

  if (itr != end()) {
    std::for_each(++itr, end(), std::mem_fun_ref(&EntryListNode::resize_file));
    return false;
  }

  return true;
}
					   
EntryList::iterator
EntryList::at_position(iterator itr, off_t offset) {
  while (itr != end() && offset >= itr->get_position() + itr->get_size())
    ++itr;

  return itr;
}

bool
EntryList::open_file(const std::string& root, FileMeta* f, const Path& p, const Path& lastPath) {
  if (p.empty())
    return false;

  Path::mkdir(root, p.begin(), --p.end(),
	      lastPath.begin(), lastPath.end());

  return
    f->prepare(MemoryChunk::prot_read | MemoryChunk::prot_write, File::o_create) ||
    f->prepare(MemoryChunk::prot_read, File::o_create);
}

inline void
EntryList::create_chunk_part(MemoryChunk& chunk, iterator itr, off_t offset, uint32_t length, int prot) {
  offset -= itr->get_position();
  length = std::min<off_t>(length, itr->get_size() - offset);

  if (offset < 0)
    throw internal_error("EntryList::get_chunk_part(...) caught a negative offset");

  if (itr->file_meta()->prepare(prot))
    chunk = itr->file_meta()->get_file().get_chunk(offset, length, prot, MemoryChunk::map_shared);
  else
    chunk.clear();
}

Chunk*
EntryList::create_chunk(off_t offset, uint32_t length, int prot) {
  MemoryChunk mc;

  if (offset + length > m_bytesSize)
    throw internal_error("Tried to access chunk out of range in EntryList");

  std::auto_ptr<Chunk> chunk(new Chunk);

  for (iterator itr = std::find_if(begin(), end(), std::bind2nd(std::mem_fun_ref(&EntryListNode::is_valid_position), offset)); length != 0; ++itr) {

    if (itr == end())
      throw internal_error("EntryList could not find a valid file for chunk");

    if (itr->get_size() == 0)
      continue;

    create_chunk_part(mc, itr, offset, length, prot);

    if (!mc.is_valid())
      return NULL;

    chunk->push_back(mc);
    offset += mc.size();
    length -= mc.size();
  }

  return !chunk->empty() ? chunk.release() : NULL;
}

}
