#include "config.h"

#include "torrent/exceptions.h"
#include "delegator_reservee.h"

namespace torrent {

// Setting state to NONE or FINISHED automagically disconnects us
// from the piece. Make sure it's done before killing the last
// object cloned from this.
void
DelegatorReservee::set_state(DelegatorState s) {
  if (m_piece == NULL)
    throw internal_error("DelegatorReservee::set_state(...) called on an invalid object");

  m_piece->set_state(s);

  if (s == DELEGATOR_NONE || s == DELEGATOR_FINISHED) {
    m_piece->set_reservee(NULL);
    m_piece = NULL;
  }
}

void
DelegatorReservee::set_parent(DelegatorPiece* p) {
}

}
