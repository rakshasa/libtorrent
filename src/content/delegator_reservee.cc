#include "config.h"

#include "torrent/exceptions.h"
#include "delegator_reservee.h"

namespace torrent {

void
DelegatorReservee::invalidate() {
  if (m_parent == NULL)
    return;

  if (m_stalled)
    m_parent->dec_stalled();

  m_parent->remove(this);
  m_parent = NULL;
}

void
DelegatorReservee::set_stalled(bool b) {
  if (b == m_stalled)
    return;

  if (m_parent == NULL)
    throw internal_error("DelegatorReservee::set_stalled(...) called on an invalid object");

  m_stalled = b;

  if (b)
    m_parent->inc_stalled();
  else
    m_parent->dec_stalled();
}

}
