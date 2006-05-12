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

#include <memory>
#include <sstream>
#include <rak/file_stat.h>

#include "torrent/exceptions.h"
#include "content.h"
#include "data/file_meta.h"
#include "data/chunk.h"
#include "data/entry_list.h"
#include "torrent/piece.h"

namespace torrent {

Content::Content() :
  m_chunkSize(1 << 16),
  m_chunkTotal(0),

  // Temporary personal hack.
  //  m_maxFileSize((uint64_t)600 << 20),
  m_maxFileSize((uint64_t)0),

  m_entryList(new EntryList) {
}

Content::~Content() {
  delete m_entryList;
}

void
Content::initialize(uint32_t chunkSize) {
  m_chunkSize = chunkSize;
  m_chunkTotal = (m_entryList->bytes_size() + chunkSize - 1) / chunkSize;

  if (m_hash.size() / 20 < m_chunkTotal)
    throw bencode_error("Torrent size and 'info:pieces' length does not match.");

  m_bitfield.resize(m_chunkTotal);

  for (EntryList::iterator itr = m_entryList->begin(); itr != m_entryList->end(); ++itr)
    (*itr)->set_range(make_index_range((*itr)->position(), (*itr)->size()));

  m_entryList->set_root_dir(".");
}

void
Content::add_file(const Path& path, uint64_t size) {
  if (m_chunkTotal)
    throw internal_error("Tried to add file to a torrent::Content that is initialized.");

  if (m_maxFileSize == 0 || size < m_maxFileSize) {
    m_entryList->push_back(path, EntryListNode::Range(), size);

  } else {
    Path newPath = path;

    for (int i = 0; size != 0; ++i) {
      std::stringstream filename;
      filename << path.back() << ".part" << i;
      newPath.back() = filename.str();

      uint64_t partSize = std::min(size, m_maxFileSize);
      size -= partSize;

      m_entryList->push_back(newPath, EntryListNode::Range(), partSize);
    }
  }
}

void
Content::set_complete_hash(const std::string& hash) {
  if (m_chunkTotal)
    throw internal_error("Tried to set hash on a torrent::Content that is initialized.");

  m_hash = hash;
}

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

  if (!m_bitfield.get(m_chunkTotal - 1) || m_entryList->bytes_size() % cs == 0)
    // The last chunk is not done, or the last chunk is the same size as the others.
    return chunks_completed() * cs;

  else
    return (chunks_completed() - 1) * cs + m_entryList->bytes_size() % cs;
}

bool
Content::is_valid_piece(const Piece& p) const {
  return
    p.index() < m_chunkTotal &&
    p.length() != 0 &&

    // Make sure offset does not overflow 32 bits.
    p.offset() < (1 << 30) &&
    p.offset() + p.length() <= chunk_index_size(p.index());
}

bool
Content::receive_chunk_hash(uint32_t index, const std::string& hash) {
  if (index >= m_chunkTotal || chunks_completed() >= m_chunkTotal)
    throw internal_error("Content::receive_chunk_hash(...) received an invalid index.");

  if (m_bitfield.get(index))
    throw internal_error("Content::receive_chunk_hash(...) received a chunk that has already been finished.");

  if (hash.empty() || std::memcmp(hash.c_str(), chunk_hash(index), 20) != 0)
    return false;

  m_bitfield.set(index);

  EntryList::iterator first = m_entryList->at_position(m_entryList->begin(), index * (off_t)m_chunkSize);
  EntryList::iterator last  = m_entryList->at_position(first, (index + 1) * (off_t)m_chunkSize - 1);

  if (last != m_entryList->end())
    last++;

  if (first == m_entryList->end())
    throw internal_error("Content::mark_done got first == m_entryList->end().");

  std::for_each(first, last, std::mem_fun(&EntryListNode::inc_completed));

  return true;
}

// Recalculate done pieces, this function clears the padding bits on the
// bitfield.
void
Content::update_done() {
  // No longer needed as the Bitfield is kept up-to-date in
  // DownloadWrapper::hash_resume_load().
  //m_bitfield.update();

  if (!m_bitfield.is_tail_cleared())
    throw internal_error("Content::update_done() called but m_bitfield's tail isn't cleared.");

  EntryList::iterator first = m_entryList->begin();
  EntryList::iterator last;

  for (Bitfield::size_type i = 0; i < m_bitfield.size_bits(); ++i)
    if (m_bitfield.get(i)) {
      first = m_entryList->at_position(first, i * (off_t)m_chunkSize);
      last = m_entryList->at_position(first, (i + 1) * (off_t)m_chunkSize - 1);

      if (last != m_entryList->end())
	last++;

      if (first == m_entryList->end())
	throw internal_error("Content::update_done() reached m_entryList->end().");

      std::for_each(first, last, std::mem_fun(&EntryListNode::inc_completed));
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
