#include "config.h"

#include "torrent/exceptions.h"
#include "delegator_reservee.h"

namespace torrent {

DelegatorReservee::~DelegatorReservee() {
  if (m_piece)
    throw internal_error("Deleted an DelegatorReservee object that has not set DELEGATOR_NONE or DELEGATOR_FINISHED");
}

void
DelegatorReservee::set_state(DelegatorState s) {
  if (m_piece == NULL)
    throw internal_error("DelegatorReservee::set_state(...) called on an invalid object");

  if (m_piece->get_reservee() != this)
    throw internal_error("DelegatorReservee::set_state(...) called on an object that has a parent piece but is not a child of that");

  m_piece->set_state(s);

  if (s == DELEGATOR_NONE || s == DELEGATOR_FINISHED) {
    m_piece->set_reservee(NULL);
    m_piece = NULL;
  }
}

}
