#include "config.h"

#include <inttypes.h>
#include <algo/algo.h>

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
  if (affinity >= 0 && 
      std::find_if(m_chunks.begin(), m_chunks.end(),
		   bool_and(eq(value((unsigned)affinity), call_member(&DelegatorChunk::get_index)),
			    call_member(ref(*this),
					&Delegator::delegate_piece,
					back_as_ref(),
					ref(target))))
      != m_chunks.end())
    return target->create();

  // High priority pieces.
  if (std::find_if(m_chunks.begin(), m_chunks.end(),
		   bool_and(eq(call_member(&DelegatorChunk::get_priority), value(Priority::HIGH)),
			    bool_and(call_member(ref(bf), &BitField::get, call_member(&DelegatorChunk::get_index)),

				     call_member(ref(*this),
						 &Delegator::delegate_piece,
						 back_as_ref(),
						 ref(target)))))
      != m_chunks.end())
    return target->create();

  // Find normal priority pieces.
  if ((target = new_chunk(bf, Priority::HIGH)))
    return target->create();

  // Normal priority pieces.
  if (std::find_if(m_chunks.begin(), m_chunks.end(),
		   bool_and(eq(call_member(&DelegatorChunk::get_priority), value(Priority::NORMAL)),
			    bool_and(call_member(ref(bf), &BitField::get, call_member(&DelegatorChunk::get_index)),
				     
				     call_member(ref(*this),
						 &Delegator::delegate_piece,
						 back_as_ref(),
						 ref(target)))))
      != m_chunks.end())
    return target->create();

  if ((target = new_chunk(bf, Priority::NORMAL)))
    return target->create();

  else if (!m_aggressive)
    return NULL;

  // Aggressive mode, look for possible downloads that already have
  // one or more queued.

  // No more than 4 per piece.
  uint16_t overlapped = 5;

  // High priority pieces, aggressive style.
  if (std::find_if(m_chunks.begin(), m_chunks.end(),
		   bool_and(eq(call_member(&DelegatorChunk::get_priority), value(Priority::HIGH)),
			    bool_and(call_member(ref(bf), &BitField::get, call_member(&DelegatorChunk::get_index)),

				     call_member(ref(*this),
						 &Delegator::delegate_aggressive,
						 back_as_ref(),
						 ref(target),
						 ref(overlapped)))))
      != m_chunks.end())
    return target->create();

  // Normal priority pieces, aggressive style
  if (std::find_if(m_chunks.begin(), m_chunks.end(),
		   bool_and(eq(call_member(&DelegatorChunk::get_priority), value(Priority::NORMAL)),
			    bool_and(call_member(ref(bf), &BitField::get, call_member(&DelegatorChunk::get_index)),
				     
				     call_member(ref(*this),
						 &Delegator::delegate_aggressive,
						 back_as_ref(),
						 ref(target),
						 ref(overlapped)))))
      != m_chunks.end())
    return target->create();

  if (target)
    return target->create();
  else
    return NULL;
}
  
void
Delegator::finished(DelegatorReservee& r) {
  if (!r.is_valid() || r.get_parent()->is_finished())
    throw internal_error("Delegator::finished(...) got object with wrong state");

  DelegatorPiece* p = r.get_parent();

  if (p == NULL)
    throw internal_error("Delegator::finished(...) got reservee with parent == NULL");

  // Temporary exception, remove when the code is rock solid. (Hah, like it ever will be;)
  if (all_finished(p->get_piece().get_index()) ||
      (*m_select.get_bitfield())[p->get_piece().get_index()])
    throw internal_error("Delegator::finished(...) called on an index that is already finished");

  p->clear();
  p->set_finished(true);

  if (all_finished(p->get_piece().get_index()))
    m_signalChunkDone.emit(p->get_piece().get_index());
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

DelegatorPiece* Delegator::new_chunk(const BitField& bf, Priority::Type p) {
  int index = m_select.find(bf, random() % bf.sizeBits(), 1024, p);

  if (index == -1)
    return NULL;

  m_select.add_ignore(index);
  m_chunks.push_back(new DelegatorChunk(index, m_slotChunkSize(index), 1 << 16, p));

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

bool
Delegator::delegate_piece(DelegatorChunk& c, DelegatorPiece*& p) {
  for (DelegatorChunk::iterator i = c.begin(); i != c.end(); ++i) {
    if (i->is_finished() || i->get_not_stalled())
      continue;

    if (i->get_reservees_size() == 0) {
      // Noone is downloading this, assign.
      p = i;
      return true;

    } else if (true) {
      // Stalled but we really want to finish this piece.
      p = i;
    }
  }
      
  return p != NULL;
}

bool
Delegator::delegate_aggressive(DelegatorChunk& c, DelegatorPiece*& p, uint16_t& overlapped) {
  for (DelegatorChunk::iterator i = c.begin(); i != c.end(); ++i) {
    if (i->is_finished() || i->get_not_stalled() >= overlapped)
      continue;

    p = i;
    overlapped = i->get_not_stalled();

    if (overlapped == 0)
      return true;
  }
      
  return false;
}


} // namespace torrent
