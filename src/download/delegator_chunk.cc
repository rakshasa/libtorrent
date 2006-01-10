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

#include "torrent/exceptions.h"
#include "delegator_chunk.h"

namespace torrent {
  
DelegatorChunk::DelegatorChunk(unsigned int index,
			       uint32_t size,
			       uint32_t piece_length,
			       Priority::Type p) :
  m_index(index),
  m_priority(p) {

  if (size == 0 || piece_length == 0)
    throw internal_error("DelegatorChunk ctor received size or piece_length equal to 0");

  m_count = (size + piece_length - 1) / piece_length;
  m_pieces = new DelegatorPiece[m_count];

  uint32_t offset = 0;

  for (iterator itr = begin(); itr != end() - 1; ++itr, offset += piece_length)
    itr->set_piece(Piece(m_index, offset, piece_length));
  
  (end() - 1)->set_piece(Piece(m_index, offset, (size % piece_length) ? size % piece_length : piece_length));
}

}
