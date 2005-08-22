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

#include <memory>

#include "torrent/exceptions.h"
#include "content.h"
#include "data/file_meta.h"
#include "data/file_stat.h"
#include "data/storage_chunk.h"

namespace torrent {

Content::Content() :
  m_isOpen(false),
  m_completed(0),
  m_chunkSize(1 << 16),
  m_rootDir(".") {

  m_delayDownloadDone.set_slot(m_signalDownloadDone.make_slot());
  m_delayDownloadDone.set_iterator(taskScheduler.end());
}

void
Content::add_file(const Path& path, uint64_t size) {
  if (is_open())
    throw internal_error("Tried to add file to Content that is open");

  m_entryList.push_back(path, make_index_range(get_bytes_size(), size), size);
}

void
Content::set_complete_hash(const std::string& hash) {
  if (is_open())
    throw internal_error("Tried to set complete hash on Content that is open");

  m_hash = hash;
}

void
Content::set_root_dir(const std::string& dir) {
  if (is_open())
    throw internal_error("Tried to set root directory on Content that is open");

  m_rootDir = dir;
}

std::string
Content::get_hash(unsigned int index) {
  if (!is_open())
    throw internal_error("Tried to get chunk hash from Content that is not open");

  if (index >= get_chunk_total())
    throw internal_error("Tried to get chunk hash from Content that is out of range");

  return m_hash.substr(index * 20, 20);
}

uint32_t
Content::get_chunk_index_size(uint32_t index) const {
  if (m_chunkSize == 0 || index >= get_chunk_total())
    throw internal_error("Content::get_chunksize(...) called but we borked");

  if (index + 1 != get_chunk_total() || get_bytes_size() % m_chunkSize == 0)
    return m_chunkSize;
  else
    return get_bytes_size() % m_chunkSize;
}

uint64_t
Content::get_bytes_completed() {
  if (!is_open())
    return 0;

  uint64_t cs = m_chunkSize;

  if (!m_bitfield[get_chunk_total() - 1] || get_bytes_size() % cs == 0)
    // The last chunk is not done, or the last chunk is the same size as the others.
    return m_completed * cs;

  else
    return (m_completed - 1) * cs + get_bytes_size() % cs;
}

bool
Content::is_correct_size() {
  if (!is_open())
    return false;

  EntryList::iterator sItr = m_entryList.begin();
  
  while (sItr != m_entryList.end()) {
    // TODO: Throw or return false?
    if (!sItr->file_meta()->prepare(MemoryChunk::prot_read))
      return false;

    if (sItr->get_size() != FileStat(sItr->file_meta()->get_file().fd()).get_size())
      return false;

    ++sItr;
  }

  return true;
}

bool
Content::is_valid_piece(const Piece& p) const {
  return
    (uint32_t)p.get_index() < get_chunk_total() &&

    p.get_length() != 0 &&
    p.get_length() < (1 << 17) &&

    // Make sure offset does not overflow 32 bits.
    p.get_offset() < (1 << 30) &&
    p.get_offset() + p.get_length() <= get_chunk_index_size(p.get_index());
}

ChunkListNode*
Content::get_chunk(uint32_t index, int prot) {
  StorageChunk* node;

  if (!m_chunkList.has_chunk(index, prot) &&
      (node = get_storage_chunk(index, prot)))
    m_chunkList.insert(index, node);
    
  return m_chunkList.bind(index);
}

void
Content::open() {
  m_entryList.open(m_rootDir);
  m_chunkList.resize(get_chunk_total());

  m_bitfield = BitField(get_chunk_total());

  if (m_hash.size() / 20 != get_chunk_total())
    throw internal_error("Content::open(...): Chunk count does not match hash count");

  m_isOpen = true;
}

void
Content::close() {
  m_isOpen = false;

  m_entryList.close();
  m_chunkList.clear();

  m_completed = 0;
  m_bitfield = BitField();
  taskScheduler.erase(&m_delayDownloadDone);
}

void
Content::resize() {
  if (!is_open())
    return;

  m_entryList.resize_all();
}

void
Content::mark_done(uint32_t index) {
  if (index >= get_chunk_total())
    throw internal_error("Content::mark_done received index out of range");
    
  if (m_bitfield[index])
    throw internal_error("Content::mark_done received index that has already been marked as done");
  
  if (m_completed >= get_chunk_total())
    throw internal_error("Content::mark_done called but m_completed >= get_chunk_total()");

  m_bitfield.set(index, true);
  m_completed++;

  mark_done_file(m_entryList.begin(), index);

  // We delay emitting the signal to allow the delegator to clean
  // up. If we do a straight call it would cause problems for
  // clients that wish to close and reopen the torrent, as
  // HashQueue, Delegator etc shouldn't be cleaned up at this point.
  if (m_completed == get_chunk_total() &&
      !m_delayDownloadDone.get_slot().blocked() &&
      !taskScheduler.is_scheduled(&m_delayDownloadDone))
    taskScheduler.insert(&m_delayDownloadDone, Timer::cache());
}

// Recalculate done pieces, this function clears the padding bits on the
// bitfield.
void
Content::update_done() {
  m_bitfield.cleanup();
  m_completed = m_bitfield.count();

  EntryList::iterator itr = m_entryList.begin();

  for (BitField::size_t i = 0; i < m_bitfield.size_bits(); ++i)
    if (m_bitfield[i])
      itr = mark_done_file(itr, i);
}

StorageChunk*
Content::get_storage_chunk(uint32_t b, int prot) {
  MemoryChunk mc;

  off_t first = get_chunk_position(b);
  off_t last = std::min(get_chunk_position(b + 1), get_bytes_size());

  if (first >= get_bytes_size())
    throw internal_error("Tried to access chunk out of range in EntryList");

  std::auto_ptr<StorageChunk> chunk(new StorageChunk(b));

  for (EntryList::iterator itr = std::find_if(m_entryList.begin(), m_entryList.end(), std::bind2nd(std::mem_fun_ref(&EntryListNode::is_valid_position), first));
       first != last; ++itr) {

    if (itr == m_entryList.end())
      throw internal_error("EntryList could not find a valid file for chunk");

    if (itr->get_size() == 0)
      continue;

    if (!(mc = get_storage_chunk_part(itr, first, last - first, prot)).is_valid())
      return NULL;

    chunk->push_back(mc);
    first += mc.size();
  }

  if (chunk->get_size() != last - get_chunk_position(b))
    throw internal_error("EntryList::get_chunk didn't get a chunk with the correct size");

  return chunk.release();
}

MemoryChunk
Content::get_storage_chunk_part(EntryList::iterator itr, off_t offset, uint32_t length, int prot) {
  offset -= itr->get_position();
  length = std::min<off_t>(length, itr->get_size() - offset);

  if (offset < 0)
    throw internal_error("EntryList::get_chunk_part(...) caught a negative offset");

  if (length == 0)
    throw internal_error("EntryList::get_chunk_part(...) caught a piece with 0 lenght");

  if (length > m_chunkSize)
    throw internal_error("EntryList::get_chunk_part(...) caught an excessively large piece");

  if (!itr->file_meta()->prepare(prot))
    return MemoryChunk();

  return itr->file_meta()->get_file().get_chunk(offset, length, prot, MemoryChunk::map_shared);
}

}
