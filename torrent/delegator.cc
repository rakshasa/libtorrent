#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>

#include "torrent/exceptions.h"
#include "general.h"
#include "delegator.h"
#include "download_state.h"
#include "content/delegator_reservee.h"

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

Delegator::~Delegator() {
  while (!m_chunks.empty()) {
    std::for_each(m_chunks.front().m_pieces.begin(), m_chunks.front().m_pieces.end(),
		  delete_on());

    m_chunks.pop_front();
  }
}

bool Delegator::interested(const BitField& bf) {
  // TODO: Unmark some of the downloading chunks.

  BitField b(bf);
  
  return !b.notIn(m_state->content().get_bitfield()).zero();
}

bool Delegator::interested(int index) {
  if (index < 0 && (unsigned)index >= m_state->content().get_bitfield().sizeBits())
    throw internal_error("Delegator::interested received index out of range");

  return !m_state->content().get_bitfield()[index];
}

DelegatorReservee*
Delegator::delegate(const BitField& bf, int affinity) {
  // TODO: Make sure we don't queue the same piece several time on the same peer when
  // it timeout cancels them.
  
  DelegatorPiece* target = NULL;

  // Find a piece that is not queued by anyone. "" and NONE.
  std::find_if(m_chunks.begin(), m_chunks.end(),

	       bool_and(call_member(ref(bf), &BitField::get, member(&Chunk::m_index)),
			
			find_if_on(member(&Chunk::m_pieces),
				   
				   eq(call_member(&DelegatorPiece::get_state), value(DELEGATOR_NONE)),
				   
				   assign_ref(target, back_as_value()))));
  
  if (target)
    return new DelegatorReservee(target);

  // else find a new chunk
  target = newChunk(bf);
  
  if (target)
    return new DelegatorReservee(target);

//   // else find a piece that is queued, but cancelled. "*" and NONE.
//   std::find_if(m_chunks.begin(), m_chunks.end(),

// 	       bool_and(call_member(ref(bf), &BitField::get, member(&Chunk::m_index)),

// 			find_if_on(member(&Chunk::m_pieces),
				   
// 				   bool_and(eq(member(&PieceInfo::m_state), value(NONE)),
// 					    bool_not(contained_in(ref(pieces),
// 								  member(&PieceInfo::m_piece)))),
				   
// 				   assign_ref(target, back_as_ptr()))));

//   if (target)
//     goto DC_designate_return;

//   // TODO: Write this asap
//   //else if (many chunks left

//   // else find piece that is being downloaded. "*" and DOWNLOADING.
//   // TODO: This will only happen when there are a few pieces left. FIXME
//   std::find_if(m_chunks.begin(), m_chunks.end(),

// 	       bool_and(call_member(ref(bf), &BitField::get, member(&Chunk::m_index)),

// 			find_if_on(member(&Chunk::m_pieces),
				   
// 				   bool_and(eq(member(&PieceInfo::m_state), value(DOWNLOADING)),
// 					    bool_not(contained_in(ref(pieces),
// 								  member(&PieceInfo::m_piece)))),
				   
// 				   assign_ref(target, back_as_ptr()))));

//   if (target)
//     goto DC_designate_return;

  return NULL;
}
  
bool
Delegator::downloading(DelegatorReservee& r) {
  if (r.is_valid()) {
    if (r.get_state() != DELEGATOR_QUEUED && r.get_state() != DELEGATOR_STALLED)
      throw internal_error("Delegator::downloading(...) got object with wrong state");
    
    r.set_state(DELEGATOR_DOWNLOADING);

    return true;

  } else {
    // We are guaranteed that the piece is still not finished.
    DelegatorPiece* pi = find_piece(r.get_piece());

    return pi && pi->get_state() != DELEGATOR_FINISHED;
  }
}
  
bool
Delegator::finished(DelegatorReservee& r) {
  DelegatorPiece* p = find_piece(r.get_piece());

  if (p == NULL)
    throw internal_error("Delegator::finished called with wrong piece");
    
  if (p->get_state() == DELEGATOR_FINISHED)
    throw internal_error("Delegator::finished called on piece that is already marked FINISHED");

  if (p != r.get_parent())
    throw internal_error("Delegator::finished got a mismatched reservee and piece");

  r.set_state(DELEGATOR_FINISHED);

  return all_state(r.get_piece().c_index(), DELEGATOR_FINISHED);
}

// If clear is set, clear the id of the piece so it is designated as if it was
// never touched.

void
Delegator::cancel(DelegatorReservee& r) {
  r.set_state(DELEGATOR_NONE);
}

void Delegator::done(int index) {
  std::list<Chunk>::iterator itr = std::find_if(m_chunks.begin(), m_chunks.end(),
						eq(&Chunk::m_index, value(index)));

  if (itr != m_chunks.end()) {
    m_select.remove_ignore(itr->m_index);

    std::for_each(itr->m_pieces.begin(), itr->m_pieces.end(), delete_on());

    m_chunks.erase(itr);
  }
}

void Delegator::redo(int index) {
  // TODO: Download pieces from the other clients and try again. Swap out pieces
  // from one id at the time.
  done(index);
}

DelegatorPiece* Delegator::newChunk(const BitField& bf) {
  BitField available(bf);
  available.notIn(m_state->content().get_bitfield());

  // Make sure we don't try downloading a chunk we already got in the list.
  std::for_each(m_chunks.begin(), m_chunks.end(),
		call_member(ref(available),
			    &BitField::set,

			    member(&Chunk::m_index),
			    value(false)));

  int index = m_select.find(bf, random() % bf.sizeBits(), 1024);

  if (index == -1)
    return NULL;

  m_select.add_ignore(index);

  // If index is the last piece, and the last piece is not divisible by chunk size
  // then set size to the size of the whole torrent modulo chunk size.
  unsigned int size =
    (unsigned)index + 1 == m_state->content().get_storage().get_chunkcount() &&
    (m_state->content().get_size() % m_state->content().get_storage().get_chunksize()) ?

    (m_state->content().get_size() % m_state->content().get_storage().get_chunksize()) :
    m_state->content().get_storage().get_chunksize();

  std::list<Chunk>::iterator itr = m_chunks.insert(m_chunks.end(), Chunk(index));

  for (int i = 0, e = size; i <= e - (1 << 15); i += 1 << 15) {
    itr->m_pieces.push_back(new DelegatorPiece);
    itr->m_pieces.back()->set_piece(Piece(index, i, 1 << 15));
  }

  unsigned int left = size % (1 << 15);

  if (left) {
    itr->m_pieces.push_back(new DelegatorPiece);
    itr->m_pieces.back()->set_piece(Piece(index, size - left, left));
  }

  return itr->m_pieces.front();
}

DelegatorPiece*
Delegator::find_piece(const Piece& p) {
  std::list<Chunk>::iterator c = std::find_if(m_chunks.begin(), m_chunks.end(),
					      eq(member(&Chunk::m_index), value(p.c_index())));

  if (c == m_chunks.end())
    return NULL;

  std::list<DelegatorPiece*>::iterator d = std::find_if(c->m_pieces.begin(), c->m_pieces.end(),
							eq(call_member(&DelegatorPiece::get_piece), ref(p)));

  if (d != c->m_pieces.end())
    return *d;
  else
    return NULL;
}
  
bool
Delegator::all_state(int index, DelegatorState s) {
  std::list<Chunk>::iterator c = std::find_if(m_chunks.begin(), m_chunks.end(),
					      eq(member(&Chunk::m_index), value(index)));

  return c != m_chunks.end() &&
    std::find_if(c->m_pieces.begin(), c->m_pieces.end(),
		 neq(call_member(&DelegatorPiece::get_state), value(s)))
    == c->m_pieces.end();
}

} // namespace torrent
