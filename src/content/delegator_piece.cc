#include "config.h"

#include "torrent/exceptions.h"
#include "delegator_piece.h"
#include "delegator_reservee.h"

#include <algo/algo.h>

using namespace algo;

namespace torrent {

DelegatorPiece::~DelegatorPiece() {
  if (m_reservees.size())
    throw internal_error("DelegatorPiece dtor called on an object that still has reservees");

  if (m_stalled)
    throw internal_error("DelegatorPiece dtor detected bad m_stalled count");
}

DelegatorReservee*
DelegatorPiece::create() {
  if (m_finished)
    throw internal_error("DelegatorPiece::create() called on a finished object");

  m_reservees.reserve(m_reservees.size() + 1);
  m_reservees.push_back(new DelegatorReservee(this));

  return m_reservees.back();
}

void
DelegatorPiece::clear() {
  // Inefficient but should seldomly be called on objects with more than 2-3
  // reservees.

  while (!m_reservees.empty())
    m_reservees.front()->invalidate();
}

void
DelegatorPiece::remove(DelegatorReservee* r) {
  Reservees::iterator itr = std::find(m_reservees.begin(), m_reservees.end(), r);

  if (itr == m_reservees.end())
    throw internal_error("DelegatorPiece::remove(...) did not find the reservee");

  m_reservees.erase(itr);
}

}
