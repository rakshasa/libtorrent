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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_DELEGATOR_CHUNK_H
#define LIBTORRENT_DELEGATOR_CHUNK_H

#include <inttypes.h>

#include "delegator_piece.h"
#include "priority.h"

namespace torrent {

class DelegatorChunk {
public:
  typedef DelegatorPiece* iterator;
  typedef const DelegatorPiece* const_iterator;

  DelegatorChunk(unsigned int index,
		 uint32_t size,
		 uint32_t piece_length,
		 Priority::Type p);
  ~DelegatorChunk()                                   { delete [] m_pieces; }

  unsigned int        get_index()                     { return m_index; }
  unsigned int        get_piece_count()               { return m_count; }
  Priority::Type      get_priority()                  { return m_priority; }

  DelegatorPiece*     begin()                         { return m_pieces; }
  DelegatorPiece*     end()                           { return m_pieces + m_count; }

  DelegatorPiece&     operator[] (unsigned int index) { return m_pieces[index]; }

private:
  DelegatorChunk(const DelegatorChunk&);
  void operator= (const DelegatorChunk&);

  unsigned int    m_index;
  unsigned int    m_count;

  Priority::Type  m_priority;
  DelegatorPiece* m_pieces;
};

}

#endif
