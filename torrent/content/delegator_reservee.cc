#include "config.h"

#include "torrent/exceptions.h"
#include "delegator_reservee.h"

namespace torrent {

DelegatorReservee::DelegatorReservee(DelegatorPiece* p) :
  m_parent(p),
  m_piece(p ? p->get_piece() : Piece()) {

  if (p == NULL)
    return;

  if (p->get_reservee())
    throw internal_error("DelegatorReservee ctor received a DelegatorPiece that already has a reservee");

  p->set_reservee(this);
}

DelegatorReservee::~DelegatorReservee() {
  if (m_parent)
    throw internal_error("Deleted an DelegatorReservee object that has not set DELEGATOR_NONE or DELEGATOR_FINISHED");
}

void
DelegatorReservee::set_state(DelegatorState s) {
  if (m_parent == NULL)
    throw internal_error("DelegatorReservee::set_state(...) called on an invalid object");

  if (m_parent->get_reservee() != this)
    throw internal_error("DelegatorReservee::set_state(...) called on an object that has a parent piece but is not a child of that");

  m_parent->set_state(s);

  if (s == DELEGATOR_NONE || s == DELEGATOR_FINISHED) {
    m_parent->set_reservee(NULL);
    m_parent = NULL;
  }
}

}
