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

#include <inttypes.h>

#include "torrent/exceptions.h"
#include "download/delegator_chunk.h"
#include "download/delegator_reservee.h"
#include "utils/bitfield.h"

#include "delegator.h"

namespace torrent {

struct DelegatorCheckAffinity {
  DelegatorCheckAffinity(Delegator* delegator, DelegatorPiece** target, unsigned int index) :
    m_delegator(delegator), m_target(target), m_index(index) {}

  bool operator () (DelegatorChunk* d) {
    return
      m_index == d->get_index() &&
      (*m_target = m_delegator->delegate_piece(d)) != NULL;
  }

  Delegator*          m_delegator;
  DelegatorPiece**    m_target;
  unsigned int        m_index;
};

struct DelegatorCheckPriority {
  DelegatorCheckPriority(Delegator* delegator, DelegatorPiece** target, Priority::Type p, const BitField* bf) :
    m_delegator(delegator), m_target(target), m_priority(p), m_bitfield(bf) {}

  bool operator () (DelegatorChunk* d) {
    return
      m_priority == d->priority() &&
      m_bitfield->get(d->get_index()) &&
      (*m_target = m_delegator->delegate_piece(d)) != NULL;
  }

  Delegator*          m_delegator;
  DelegatorPiece**    m_target;
  Priority::Type      m_priority;
  const BitField*     m_bitfield;
};

// TODO: Should this ensure we don't download pieces that are priority off?
struct DelegatorCheckAggressive {
  DelegatorCheckAggressive(Delegator* delegator, DelegatorPiece** target, uint16_t* o, const BitField* bf) :
    m_delegator(delegator), m_target(target), m_overlapp(o), m_bitfield(bf) {}

  bool operator () (DelegatorChunk* d) {
    DelegatorPiece* tmp;

    if (!m_bitfield->get(d->get_index()) ||
	d->priority() == Priority::STOPPED ||
	(tmp = m_delegator->delegate_aggressive(d, m_overlapp)) == NULL)
      return false;

    *m_target = tmp;
    return m_overlapp == 0;
  }

  Delegator*          m_delegator;
  DelegatorPiece**    m_target;
  uint16_t*           m_overlapp;
  const BitField*     m_bitfield;
};

void Delegator::clear() {
  std::for_each(m_chunks.begin(), m_chunks.end(), rak::call_delete<DelegatorChunk>());

  m_chunks.clear();
  m_select.clear();
  m_aggressive = false;
}

DelegatorReservee*
Delegator::delegate(const BitField& bf, int affinity) {
  // TODO: Make sure we don't queue the same piece several time on the same peer when
  // it timeout cancels them.
  DelegatorPiece* target = NULL;

  // Find piece with same index as affinity. This affinity should ensure that we
  // never start another piece while the chunk this peer used to download is still
  // in progress.
  //
  // TODO: What if the hash failed? Don't want data from that peer again.
  if (affinity >= 0 && 
      std::find_if(m_chunks.begin(), m_chunks.end(),
		   DelegatorCheckAffinity(this, &target, affinity))		   
      != m_chunks.end())
    return target->create();

  // High priority pieces.
  if (std::find_if(m_chunks.begin(), m_chunks.end(),
		   DelegatorCheckPriority(this, &target, Priority::HIGH, &bf))
      != m_chunks.end())
    return target->create();

  // Find normal priority pieces.
  if ((target = new_chunk(bf, Priority::HIGH, affinity)))
    return target->create();

  // Normal priority pieces.
  if (std::find_if(m_chunks.begin(), m_chunks.end(),
		   DelegatorCheckPriority(this, &target, Priority::NORMAL, &bf))
      != m_chunks.end())
    return target->create();

  if ((target = new_chunk(bf, Priority::NORMAL, affinity)))
    return target->create();

  if (!m_aggressive)
    return NULL;

  // Aggressive mode, look for possible downloads that already have
  // one or more queued.

  // No more than 4 per piece.
  uint16_t overlapped = 5;

  std::find_if(m_chunks.begin(), m_chunks.end(),
	       DelegatorCheckAggressive(this, &target, &overlapped, &bf));

  return target ? target->create() : NULL;
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
      (*m_select.bitfield())[p->get_piece().get_index()])
    throw internal_error("Delegator::finished(...) called on an index that is already finished");

  p->clear();
  p->set_finished(true);

  if (all_finished(p->get_piece().get_index()))
    m_slotChunkDone(p->get_piece().get_index());
}

void
Delegator::done(unsigned int index) {
  Chunks::iterator itr = std::find_if(m_chunks.begin(), m_chunks.end(),
				      rak::equal(index, std::mem_fun(&DelegatorChunk::get_index)));

  if (itr == m_chunks.end())
    throw internal_error("Called Delegator::done(...) with an index that is not in the Delegator");

  m_select.remove_ignore((*itr)->get_index());

  delete *itr;
  m_chunks.erase(itr);
}

void
Delegator::redo(unsigned int index) {
  // TODO: Download pieces from the other clients and try again. Swap out pieces
  // from one id at the time.
  done(index);
}

DelegatorPiece*
Delegator::new_chunk(const BitField& bf, Priority::Type p, int affinity) {
  int index = m_select.find(bf,
			    (affinity >= 0 ? ++affinity : random()) % bf.size_bits(),
			    1024,
			    p);

  if (index == -1)
    return NULL;

  m_select.add_ignore(index);
  m_chunks.push_back(new DelegatorChunk(index, m_slotChunkSize(index), 1 << 16, p));

  return (*m_chunks.rbegin())->begin();
}

DelegatorPiece*
Delegator::find_piece(const Piece& p) {
  Chunks::iterator c = std::find_if(m_chunks.begin(), m_chunks.end(),
				    rak::equal((unsigned int)p.get_index(), std::mem_fun(&DelegatorChunk::get_index)));
  
  if (c == m_chunks.end())
    return NULL;

  DelegatorChunk::iterator d = std::find_if((*c)->begin(), (*c)->end(),
					    rak::equal(p, std::mem_fun_ref(&DelegatorPiece::get_piece)));

  return d != (*c)->end() ? d : NULL;
}
  
bool
Delegator::all_finished(int index) {
  Chunks::iterator c = std::find_if(m_chunks.begin(), m_chunks.end(),
				    rak::equal((unsigned int)index, std::mem_fun(&DelegatorChunk::get_index)));

  return
    c != m_chunks.end() &&
    std::find_if((*c)->begin(), (*c)->end(),
		 std::not1(std::mem_fun_ref(&DelegatorPiece::is_finished)))
    == (*c)->end();
}

DelegatorPiece*
Delegator::delegate_piece(DelegatorChunk* c) {
  DelegatorPiece* p = NULL;

  for (DelegatorChunk::iterator i = c->begin(); i != c->end(); ++i) {
    if (i->is_finished() || i->get_not_stalled())
      continue;

    if (i->get_reservees_size() == 0) {
      // Noone is downloading this, assign.
      return &*i;

    } else if (true) {
      // Stalled but we really want to finish this piece.
      p = &*i;
    }
  }
      
  return p;
}

DelegatorPiece*
Delegator::delegate_aggressive(DelegatorChunk* c, uint16_t* overlapped) {
  DelegatorPiece* p = NULL;

  for (DelegatorChunk::iterator i = c->begin(); i != c->end() && *overlapped != 0; ++i)
    if (!i->is_finished() && i->get_not_stalled() < *overlapped) {
      p = &*i;
      *overlapped = i->get_not_stalled();
    }
      
  return p;
}

} // namespace torrent
