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
  std::for_each(m_chunks.begin(), m_chunks.end(), delete_on());
}

DelegatorReservee*
Delegator::delegate(const BitField& bf, int affinity) {
  // TODO: Make sure we don't queue the same piece several time on the same peer when
  // it timeout cancels them.
  
  DelegatorPiece* target = NULL;

  // Find piece with same index as affinity. This affinity should ensure that we
  // never start another piece while the chunk this peer used to download is still
  // in progress.
  if (affinity >= 0) 
    std::find_if(m_chunks.begin(), m_chunks.end(),
		 bool_and(eq(value((unsigned)affinity), call_member(&DelegatorChunk::get_index)),
			  
			  find_if_on(back_as_ref_t<DelegatorChunk>(),
				     bool_and(bool_not(call_member(&DelegatorPiece::is_finished)),
					      bool_not(call_member(&DelegatorPiece::get_reservees_size))),
				     assign_ref(target, back_as_ptr()))));
	       
  if (target)
    return target->create();

  std::find_if(m_chunks.begin(), m_chunks.end(),
	       bool_and(call_member(ref(bf), &BitField::get, call_member(&DelegatorChunk::get_index)),
			
			find_if_on(back_as_ref_t<DelegatorChunk>(),
				   bool_and(bool_not(call_member(&DelegatorPiece::is_finished)),
					    bool_not(call_member(&DelegatorPiece::get_reservees_size))),
				   assign_ref(target, back_as_ptr()))));
  
  if (target)
    return target->create();

  // Download stalled pieces.
  std::find_if(m_chunks.begin(), m_chunks.end(),
	       bool_and(call_member(ref(bf), &BitField::get, call_member(&DelegatorChunk::get_index)),
			
			find_if_on(back_as_ref_t<DelegatorChunk>(),

				   bool_and(bool_not(call_member(&DelegatorPiece::is_finished)),
					    bool_not(call_member(&DelegatorPiece::get_not_stalled))),

				   assign_ref(target, back_as_ptr()))));
  
  if (target)
    return target->create();

  // else find a new chunk
  target = new_chunk(bf);
  
  if (target)
    return target->create();

  return NULL;
}
  
bool
Delegator::downloading(DelegatorReservee& r) {
  if (!r.is_valid())
    throw internal_error("Delegator::downloading(...) got an invalid reservee");

  if (r.get_parent()->is_finished())
    throw internal_error("Delegator::downloading(...) got an reservee that is already finished");

  return true;
}

void
Delegator::finished(DelegatorReservee& r) {
  if (!r.is_valid() || r.get_parent()->is_finished())
    throw internal_error("Delegator::finished(...) got object with wrong state");

  DelegatorPiece* p = r.get_parent();

  p->clear();
  p->set_finished(true);

  if (all_finished(p->get_piece().get_index()))
    m_signalChunkDone.emit(p->get_piece().get_index());
}

void
Delegator::cancel(DelegatorReservee& r) {
  r.invalidate();
}

void Delegator::done(int index) {
  Chunks::iterator itr = std::find_if(m_chunks.begin(), m_chunks.end(),
				      eq(call_member(&DelegatorChunk::get_index), value((unsigned int)index)));

  if (itr == m_chunks.end())
    //throw internal_error("Called Delegator::done(...) with an index that is not in the Delegator");
    return;

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
  m_chunks.push_back(new DelegatorChunk(index, m_slotChunkSize(index), 1 << 16));

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
Delegator::all_finished(int index) {
  Chunks::iterator c = std::find_if(m_chunks.begin(), m_chunks.end(),
				    eq(call_member(&DelegatorChunk::get_index), value((unsigned)index)));

  return c != m_chunks.end() &&
    std::find_if((*c)->begin(), (*c)->end(),
		 bool_not(call_member(&DelegatorPiece::is_finished)))
    == (*c)->end();
}

} // namespace torrent
