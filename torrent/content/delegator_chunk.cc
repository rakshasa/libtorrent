#include "config.h"

#include "torrent/exceptions.h"
#include "delegator_chunk.h"

namespace torrent {
  
DelegatorChunk::DelegatorChunk(unsigned int index,
			       uint32_t size,
			       uint32_t piece_length) :
  m_index(index) {

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
