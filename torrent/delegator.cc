#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>

#include "exceptions.h"
#include "general.h"
#include "delegator.h"
#include "download_state.h"

#include <algo/algo.h>
#include <iostream>
#include <limits>

// id = "",  state = NONE - Noone has this queued, because noone touched it or we
//                          received choke.
//
// id = "*", state = NONE - One or more peers tried to download, but timed out.
//                          Is still queued by someone. Designate if no pieces remain
//                          and newChunk fails.(?)
//
// id = "X", state = DOWNLOADING - Is in X's download queue. Can also be in other
//                                 peer's queues. Don't designate.
//
// id = "X", state = FINISHED - X finished this piece.

using namespace algo;

namespace torrent {

bool Delegator::interested(const BitField& bf) {
  // TODO: Unmark some of the downloading chunks.

  BitField b(bf);
  
  return !b.notIn(m_state->files().bitfield()).zero();
}

bool Delegator::interested(int index) {
  if (index < 0 && (unsigned)index >= m_state->files().bitfield().sizeBits())
    throw internal_error("Delegator::interested received index out of range");

  return !m_state->files().bitfield()[index];
}

bool Delegator::delegate(const std::string& id, const BitField& bf, std::list<Piece>& pieces) {
  // TODO: Make sure we don't queue the same piece several time on the same peer when
  // it timeout cancels them.
  
  PieceInfo* target = NULL;

  // Find a piece that is not queued by anyone. "" and NONE.
  std::find_if(m_chunks.begin(), m_chunks.end(),

	       bool_and(call_member(ref(bf), &BitField::get, member(&Chunk::m_index)),
			
			find_if_on(member(&Chunk::m_pieces),
				   
				   bool_and(eq(member(&PieceInfo::m_state), value(NONE)),
					    eq(member(&PieceInfo::m_id), value(""))),
				   
				   assign_ref(target, back_as_ptr()))));
  
  if (target)
    goto DC_designate_return;

  // else find a new chunk
  target = newChunk(bf);
  
  if (target)
    goto DC_designate_return;

  // else find a piece that is queued, but cancelled. "*" and NONE.
  std::find_if(m_chunks.begin(), m_chunks.end(),

	       bool_and(call_member(ref(bf), &BitField::get, member(&Chunk::m_index)),

			find_if_on(member(&Chunk::m_pieces),
				   
				   bool_and(eq(member(&PieceInfo::m_state), value(NONE)),
					    bool_not(contained_in(ref(pieces),
								  member(&PieceInfo::m_piece)))),
				   
				   assign_ref(target, back_as_ptr()))));

  if (target)
    goto DC_designate_return;

  // TODO: Write this asap
  //else if (many chunks left

  // else find piece that is being downloaded. "*" and DOWNLOADING.
  // TODO: This will only happen when there are a few pieces left. FIXME
  std::find_if(m_chunks.begin(), m_chunks.end(),

	       bool_and(call_member(ref(bf), &BitField::get, member(&Chunk::m_index)),

			find_if_on(member(&Chunk::m_pieces),
				   
				   bool_and(eq(member(&PieceInfo::m_state), value(DOWNLOADING)),
					    bool_not(contained_in(ref(pieces),
								  member(&PieceInfo::m_piece)))),
				   
				   assign_ref(target, back_as_ptr()))));

  if (target)
    goto DC_designate_return;

  return false;

  DC_designate_return:

  target->m_id = id;
  target->m_state = DOWNLOADING;
  
  pieces.push_back(target->m_piece);

  return true;
}
  
bool Delegator::downloading(const std::string& id, const Piece& piece) {
  PieceInfo* pi = NULL;
  
  std::find_if(m_chunks.begin(), m_chunks.end(),
	       find_if_on(member(&Chunk::m_pieces),

			  eq(ref(piece), member(&PieceInfo::m_piece)),

			  assign_ref(pi, back_as_ptr())));

  if (pi == NULL ||
      pi->m_state == FINISHED) {
    return false;

  } else if (pi->m_state == NONE) {
    pi->m_state = DOWNLOADING;
    pi->m_id = id;
  }

  return true;
}
  
bool Delegator::finished(const std::string& id, const Piece& piece) {
  std::list<Chunk>::iterator c = std::find_if(m_chunks.begin(), m_chunks.end(),
					      eq(member(&Chunk::m_index), value(piece.indexC())));

  if (c == m_chunks.end())
    throw internal_error("Delegator::finished called on wrong index");

  std::list<PieceInfo>::iterator p = std::find_if(c->m_pieces.begin(), c->m_pieces.end(),
						  eq(member(&PieceInfo::m_piece), ref(piece)));

  if (p == c->m_pieces.end())
    throw internal_error("Delegator::finished called with bad chunk position");
    
  if (p->m_state == FINISHED)
    throw internal_error("Delegator::finished called on piece that is already marked FINISHED");

  p->m_id = id;
  p->m_state = FINISHED;

  return std::find_if(c->m_pieces.begin(), c->m_pieces.end(),
		      neq(member(&PieceInfo::m_state), value(FINISHED)))
    == c->m_pieces.end();
}

// If clear is set, clear the id of the piece so it is designated as if it was
// never touched.
void Delegator::cancel(const std::string& id, const Piece& p, bool clear) {
  std::find_if(m_chunks.begin(), m_chunks.end(),
	       find_if_on(member(&Chunk::m_pieces),
			  
			  bool_and(eq(member(&PieceInfo::m_piece), ref(p)),
				   bool_and(eq(member(&PieceInfo::m_id), ref(id)),
					    neq(member(&PieceInfo::m_state), value(FINISHED)))),
					    

			  branch(assign(member(&PieceInfo::m_state), value(NONE)),
				 if_on(value(clear),
				       assign(member(&PieceInfo::m_id), value(""))))));
}

void Delegator::done(int index) {
  std::list<Chunk>::iterator itr = std::find_if(m_chunks.begin(), m_chunks.end(),
						eq(&Chunk::m_index, value(index)));

  if (itr != m_chunks.end())
    m_chunks.erase(itr);
}

void Delegator::redo(int index) {
  // TODO: Download pieces from the other clients and try again. Swap out pieces
  // from one id at the time.
  done(index);
}

Delegator::PieceInfo* Delegator::newChunk(const BitField& bf) {
  BitField available(bf);
  available.notIn(m_state->files().bitfield());

  // Make sure we don't try downloading a chunk we already got in the list.
  std::for_each(m_chunks.begin(), m_chunks.end(),
		call_member(ref(available),
			    &BitField::set,

			    member(&Chunk::m_index),
			    value(false)));


  int index = findChunk(available);

  if (index == -1)
    return NULL;

  unsigned int size = (unsigned)index + 1 != m_state->files().storage().get_chunkcount() ?
    m_state->files().storage().get_chunksize() :
    m_state->files().storage().get_size() % m_state->files().storage().get_chunksize();

  std::list<Chunk>::iterator itr = m_chunks.insert(m_chunks.end(), Chunk(index));

  for (int i = 0, e = size; i <= e - (1 << 15); i += 1 << 15)
    itr->m_pieces.push_back(PieceInfo(Piece(index, i, 1 << 15)));

  unsigned int left = size % (1 << 15);

  if (left)
    itr->m_pieces.push_back(PieceInfo(Piece(index, size - left, left)));

  return &itr->m_pieces.front();
}

int Delegator::findChunk(const BitField& bf) {
  int selectedIndex = -1;
  int selectedDistance = std::numeric_limits<int>::max();

  // TODO: Select the target with a fancy algorithm. Have a ceiling
  // setting and a base setting.
  int target = 1;

  const char *cur, *end;
  cur = end = bf.data() + random() % bf.sizeBytes();

  do {
    if (*cur)
      // This byte has some interesting chunks.
      for (int i = 0; i < 8; ++i)

	if (*cur & (1 << 7 - i) &&
	    std::abs(m_state->bfCounter().field()[(cur - bf.data()) * 8 + i] - target) < selectedDistance) {
	  // Found a closer match
	  selectedIndex = (cur - bf.data()) * 8 + i;
	  selectedDistance = std::abs(m_state->bfCounter().field()[selectedIndex] - target);

	  if (selectedDistance == 0)
	    break;
	}

    if (++cur == bf.end())
      cur = bf.data();

  } while (cur != end);
  
  return selectedIndex;
}

} // namespace torrent
