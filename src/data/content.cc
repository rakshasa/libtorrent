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
#include <rak/file_stat.h>

#include "torrent/exceptions.h"
#include "torrent/data/file.h"
#include "torrent/data/file_list.h"
#include "torrent/data/piece.h"

#include "content.h"
#include "file_meta.h"
#include "chunk.h"

namespace torrent {

// Temporary, Content will be deprecated.
uint32_t
Content::chunk_size() const {
  return m_fileList->chunk_size();
}

Content::Content() :
  m_fileList(new FileList) {
}

Content::~Content() {
  m_fileList->clear();
  delete m_fileList;
}

void
Content::initialize() {
  if (chunk_size() == 0)
    throw internal_error("Content::initialize() chunk_size() == 0.");

  m_bitfield.set_size_bits((m_fileList->size_bytes() + chunk_size() - 1) / chunk_size());

  if (m_hash.size() / 20 < chunk_total())
    throw bencode_error("Torrent size and 'info:pieces' length does not match.");

  m_fileList->set_root_dir(".");
}

void
Content::add_file(const Path& path, uint64_t size) {
  if (chunk_total())
    throw internal_error("Tried to add file to a torrent::Content that is initialized.");

  m_fileList->push_back(path, size);
}

void
Content::set_complete_hash(const std::string& hash) {
  if (chunk_total())
    throw internal_error("Tried to set hash on a torrent::Content that is initialized.");

  m_hash = hash;
}

uint32_t
Content::chunk_index_size(uint32_t index) const {
  if (index + 1 != chunk_total() || m_fileList->size_bytes() % chunk_size() == 0)
    return chunk_size();
  else
    return m_fileList->size_bytes() % chunk_size();
}

uint64_t
Content::bytes_completed() const {
  // Chunk size needs to be cast to a uint64_t for the below to work.
  uint64_t cs = chunk_size();

  if (m_bitfield.empty())
    return m_bitfield.is_all_set() ? m_fileList->size_bytes() : (chunks_completed() * cs);

  if (!m_bitfield.get(chunk_total() - 1) || m_fileList->size_bytes() % cs == 0) {
    // The last chunk is not done, or the last chunk is the same size
    // as the others.
    return chunks_completed() * cs;

  } else {
    if (chunks_completed() == 0)
      throw internal_error("Content::bytes_completed() chunks_completed() == 0.");

    return (chunks_completed() - 1) * cs + m_fileList->size_bytes() % cs;
  }
}

uint64_t
Content::bytes_left() const {
  uint64_t left = file_list()->size_bytes() - bytes_completed();

  if (left > ((uint64_t)1 << 60))
    throw internal_error("Content::bytes_left() is too large."); 

  if (chunks_completed() == chunk_total() && left != 0)
    throw internal_error("Content::bytes_left() has an invalid size."); 

  return left;
}

bool
Content::is_valid_piece(const Piece& p) const {
  return
    p.index() < chunk_total() &&
    p.length() != 0 &&

    // Make sure offset does not overflow 32 bits.
    p.offset() < (1 << 30) &&
    p.offset() + p.length() <= chunk_index_size(p.index());
}

bool
Content::receive_chunk_hash(uint32_t index, const char* hash) {
  if (m_bitfield.get(index))
    throw internal_error("Content::receive_chunk_hash(...) received a chunk that has already been finished.");

  if (m_bitfield.size_set() >= m_bitfield.size_bits())
    throw internal_error("Content::receive_chunk_hash(...) m_bitfield.size_set() >= m_bitfield.size_bits().");

  if (index >= chunk_total() || chunks_completed() >= chunk_total())
    throw internal_error("Content::receive_chunk_hash(...) received an invalid index.");

  if (std::memcmp(hash, chunk_hash(index), 20) != 0)
    return false;

  m_bitfield.set(index);
  m_fileList->inc_completed(m_fileList->begin(), index * (off_t)chunk_size(), (index + 1) * (off_t)chunk_size());

  return true;
}

// Recalculate done pieces, this function clears the padding bits on the
// bitfield.
void
Content::update_done() {
  if (!m_bitfield.is_tail_cleared())
    throw internal_error("Content::update_done() called but m_bitfield's tail isn't cleared.");

  FileList::iterator entryItr = m_fileList->begin();

  for (Bitfield::size_type index = 0; index < m_bitfield.size_bits(); ++index)
    if (m_bitfield.get(index))
      entryItr = m_fileList->inc_completed(entryItr, index * (off_t)chunk_size(), (index + 1) * (off_t)chunk_size());
}

std::pair<Chunk*,rak::error_number>
Content::create_chunk(uint32_t index, bool writable) {
  rak::error_number::clear_global();

  Chunk* c = m_fileList->create_chunk(chunk_position(index), chunk_index_size(index),
                                       MemoryChunk::prot_read | (writable ? MemoryChunk::prot_write : 0));

  return std::pair<Chunk*,rak::error_number>(c, c == NULL ? rak::error_number::current() : rak::error_number());
}

}
