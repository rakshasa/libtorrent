#include "config.h"

#include <inttypes.h>
#include <algo/algo.h>
#include <limits>

#include "torrent/exceptions.h"
#include "content/delegator_reservee.h"
#include "general.h"
#include "download_state.h"

#include "delegator.h"

using namespace algo;

namespace torrent {

Delegator::~Delegator() {
  while (!m_chunks.empty()) {
    delete m_chunks.front();
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

	       bool_and(call_member(ref(bf), &BitField::get, call_member(&DelegatorChunk::get_index)),
			
			find_if_on(back_as_ref_t<DelegatorChunk>(),
				   
				   eq(call_member(&DelegatorPiece::get_state), value(DELEGATOR_NONE)),
				   
				   assign_ref(target, back_as_ptr()))));
  
  if (target)
    return new DelegatorReservee(target);

  // else find a new chunk
  target = new_chunk(bf);
  
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
  
void
Delegator::finished(DelegatorReservee& r) {
  DelegatorPiece* p = find_piece(r.get_piece());

  if (p == NULL)
    throw internal_error("Delegator::finished called with wrong piece");
    
  if (p->get_state() == DELEGATOR_FINISHED)
    throw internal_error("Delegator::finished called on piece that is already marked FINISHED");

  if (p != r.get_parent())
    throw internal_error("Delegator::finished got a mismatched reservee and piece");

  r.set_state(DELEGATOR_FINISHED);

  if (all_state(r.get_piece().get_index(), DELEGATOR_FINISHED))
    m_signalChunkDone.emit(r.get_piece().get_index());
}

void
Delegator::cancel(DelegatorReservee& r) {
  r.set_state(DELEGATOR_NONE);
}

void Delegator::done(int index) {
  Chunks::iterator itr = std::find_if(m_chunks.begin(), m_chunks.end(),
				      eq(call_member(&DelegatorChunk::get_index), value((unsigned int)index)));

  if (itr == m_chunks.end())
    throw internal_error("Called Delegator::done(...) with an index that is not in the Delegator");

  m_select.remove_ignore((*itr)->get_index());

  delete *itr;
  m_chunks.erase(itr);
}

void Delegator::redo(int index) {
  // TODO: Download pieces from the other clients and try again. Swap out pieces
  // from one id at the time.
  done(index);
}

DelegatorPiece* Delegator::new_chunk(const BitField& bf) {
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

  m_chunks.push_back(new DelegatorChunk(index, size, 1 << 16));

  return (*m_chunks.rbegin())->begin();
}

DelegatorPiece*
Delegator::find_piece(const Piece& p) {
  Chunks::iterator c = std::find_if(m_chunks.begin(), m_chunks.end(),
				    eq(call_member(&DelegatorChunk::get_index), value((unsigned)p.get_index())));

  if (c == m_chunks.end())
    return NULL;

  DelegatorChunk::iterator d = std::find_if((*c)->begin(), (*c)->end(),
					    eq(call_member(&DelegatorPiece::get_piece), ref(p)));

  if (d != (*c)->end())
    return d;
  else
    return NULL;
}
  
bool
Delegator::all_state(int index, DelegatorState s) {
  Chunks::iterator c = std::find_if(m_chunks.begin(), m_chunks.end(),
				    eq(call_member(&DelegatorChunk::get_index), value((unsigned)index)));

  return c != m_chunks.end() &&
    std::find_if((*c)->begin(), (*c)->end(),
		 neq(call_member(&DelegatorPiece::get_state), value(s)))
    == (*c)->end();
}

} // namespace torrent
