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
#include "data/chunk.h"
#include "data/entry_list.h"
#include "data/piece.h"

namespace torrent {

Content::Content() :
  m_completed(0),
  m_chunkSize(1 << 16),
  m_chunkTotal(0),

  m_entryList(new EntryList),
  m_rootDir(".") {

  m_delayDownloadDone.set_slot(m_signalDownloadDone.make_slot());
  m_delayDownloadDone.set_iterator(taskScheduler.end());
}

Content::~Content() {
  delete m_entryList;
}

void
Content::initialize(uint32_t chunkSize) {
  m_chunkSize = chunkSize;
  m_chunkTotal = (m_entryList->bytes_size() + chunkSize - 1) / chunkSize;

  m_bitfield = BitField(m_chunkTotal);

  for (EntryList::iterator itr = m_entryList->begin(); itr != m_entryList->end(); ++itr)
    itr->set_range(make_index_range(itr->position(), itr->size()));
}

void
Content::add_file(const Path& path, uint64_t size) {
  if (m_chunkTotal)
    throw internal_error("Tried to add file to Content that is open");

  m_entryList->push_back(path, EntryListNode::Range(), size);
}

void
Content::set_complete_hash(const std::string& hash) {
  if (m_chunkTotal)
    throw internal_error("Tried to set complete hash on Content that is open");

  m_hash = hash;
}

void
Content::set_root_dir(const std::string& dir) {
//   if (m_chunkTotal)
//     throw internal_error("Tried to set root directory on Content that is open");

  m_rootDir = dir;
}

// std::string
// Content::get_hash(unsigned int index) {
//   if (index >= m_chunkTotal)
//     throw internal_error("Tried to get chunk hash from Content that is out of range");

//   return m_hash.substr(index * 20, 20);
// }

uint32_t
Content::chunk_index_size(uint32_t index) const {
  if (index + 1 != m_chunkTotal || m_entryList->bytes_size() % m_chunkSize == 0)
    return m_chunkSize;
  else
    return m_entryList->bytes_size() % m_chunkSize;
}

uint64_t
Content::bytes_completed() const {
  uint64_t cs = m_chunkSize;

  if (!m_bitfield[m_chunkTotal - 1] || m_entryList->bytes_size() % cs == 0)
    // The last chunk is not done, or the last chunk is the same size as the others.
    return m_completed * cs;

  else
    return (m_completed - 1) * cs + m_entryList->bytes_size() % cs;
}

bool
Content::is_valid_piece(const Piece& p) const {
  return
    (uint32_t)p.get_index() < m_chunkTotal &&

    p.get_length() != 0 &&
    p.get_length() < (1 << 17) &&

    // Make sure offset does not overflow 32 bits.
    p.get_offset() < (1 << 30) &&
    p.get_offset() + p.get_length() <= chunk_index_size(p.get_index());
}

void
Content::open() {
  m_entryList->open(m_rootDir);

  if (m_hash.size() / 20 != m_chunkTotal)
    throw internal_error("Content::open(...): Chunk count does not match hash count");
}

void
Content::close() {
  clear();

  m_entryList->close();
  taskScheduler.erase(&m_delayDownloadDone);
}

void
Content::clear() {
  m_completed = 0;
  m_bitfield = BitField(m_chunkTotal);
}

void
Content::resize() {
  m_entryList->resize_all();
}

void
Content::mark_done(uint32_t index) {
  if (index >= m_chunkTotal)
    throw internal_error("Content::mark_done received index out of range");
    
  if (m_bitfield[index])
    throw internal_error("Content::mark_done received index that has already been marked as done");
  
  if (m_completed >= m_chunkTotal)
    throw internal_error("Content::mark_done called but m_completed >= m_chunkTotal");

  m_bitfield.set(index, true);
  m_completed++;

  EntryList::iterator first = m_entryList->at_position(m_entryList->begin(), index * (off_t)m_chunkSize);
  EntryList::iterator last  = m_entryList->at_position(first, (index + 1) * (off_t)m_chunkSize - 1);

  if (last != m_entryList->end())
    last++;

  if (first == m_entryList->end())
    throw internal_error("Content::mark_done got first == m_entryList->end().");

  std::for_each(first, last, std::mem_fun_ref(&EntryListNode::inc_completed));

  // We delay emitting the signal to allow the delegator to clean
  // up. If we do a straight call it would cause problems for
  // clients that wish to close and reopen the torrent, as
  // HashQueue, Delegator etc shouldn't be cleaned up at this point.
  if (m_completed == m_chunkTotal &&
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

  EntryList::iterator first = m_entryList->begin();
  EntryList::iterator last;

  for (BitField::size_t i = 0; i < m_bitfield.size_bits(); ++i)
    if (m_bitfield[i]) {
      first = m_entryList->at_position(first, i * (off_t)m_chunkSize);
      last = m_entryList->at_position(first, (i + 1) * (off_t)m_chunkSize - 1);

      if (last != m_entryList->end())
	last++;

      if (first == m_entryList->end())
	throw internal_error("Content::update_done() reached m_entryList->end().");

      std::for_each(first, last, std::mem_fun_ref(&EntryListNode::inc_completed));
    }
}

std::pair<Chunk*,rak::error_number>
Content::create_chunk(uint32_t index, bool writable) {
  rak::error_number::clear_global();

  Chunk* c = m_entryList->create_chunk(chunk_position(index), chunk_index_size(index),
				       MemoryChunk::prot_read | (writable ? MemoryChunk::prot_write : 0));

  return std::pair<Chunk*,rak::error_number>(c, c == NULL ? rak::error_number::current() : rak::error_number());
}

}
