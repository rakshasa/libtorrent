#ifndef LIBTORRENT_DELEGATOR_CHUNK_H
#define LIBTORRENT_DELEGATOR_CHUNK_H

#include <inttypes.h>
#include "delegator_piece.h"

namespace torrent {

class DelegatorChunk {
public:
  typedef DelegatorPiece* iterator;
  typedef const DelegatorPiece* const_iterator;

  DelegatorChunk(unsigned int index,
		 uint32_t size,
		 uint32_t piece_length);
  ~DelegatorChunk()                                   { delete [] m_pieces; }

  unsigned int        get_index()                     { return m_index; }
  unsigned int        get_piece_count()               { return m_count; }

  DelegatorPiece*     begin()                         { return m_pieces; }
  DelegatorPiece*     end()                           { return m_pieces + m_count; }

  DelegatorPiece&     operator[] (unsigned int index) { return m_pieces[index]; }

private:
  DelegatorChunk(const DelegatorChunk&);
  void operator= (const DelegatorChunk&);

  unsigned int    m_index;
  unsigned int    m_count;

  DelegatorPiece* m_pieces;
};

}

#endif
