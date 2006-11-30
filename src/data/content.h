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

#ifndef LIBTORRENT_CONTENT_H
#define LIBTORRENT_CONTENT_H

#include <inttypes.h>
#include <string>
#include <rak/error_number.h>

#include "torrent/bitfield.h"
#include "globals.h"

namespace torrent {

// Since g++ uses reference counted strings, it's cheaper to just hand
// over bencode's string.

// The ranges in the ContentFile elements spans from the first chunk
// they have data on, to the last plus one. This means the range end
// minus one of one file can be the start of one or more other file
// ranges.

class ChunkListNode;
class FileList;
class Path;
class Piece;

class Content {
public:
  typedef std::pair<uint32_t, uint32_t>       Range;
  typedef std::pair<Chunk*,rak::error_number> CreateChunk;

  Content();
  ~Content();

  void                   initialize();

  // Do not modify chunk size after files have been added.
  void                   add_file(const Path& path, uint64_t size);

  const std::string&     complete_hash()                                { return m_hash; }
  void                   set_complete_hash(const std::string& hash);

  uint32_t               chunks_completed() const                       { return m_bitfield.size_set(); }

  uint64_t               bytes_completed() const;
  uint64_t               bytes_left() const;

  uint32_t               chunk_total() const                            { return m_bitfield.size_bits(); }
  const char*            chunk_hash(unsigned int index)                 { return m_hash.c_str() + 20 * index; }

  uint32_t               chunk_size() const;

  uint32_t               chunk_index_size(uint32_t index) const;
  off_t                  chunk_position(uint32_t c) const               { return c * chunk_size(); }

  Bitfield*              bitfield()                                     { return &m_bitfield; }

  FileList*              file_list()                                    { return m_fileList; }
  const FileList*        file_list() const                              { return m_fileList; }

  bool                   is_done() const                                { return chunks_completed() == chunk_total(); }
  bool                   is_valid_piece(const Piece& p) const;

  bool                   has_chunk(uint32_t index) const                { return m_bitfield.get(index); }
  CreateChunk            create_chunk(uint32_t index, bool writable);

  bool                   receive_chunk_hash(uint32_t index, const char* hash);

  void                   update_done();

private:
  FileList*              m_fileList;
  Bitfield               m_bitfield;

  std::string            m_hash;
};

}

#endif
