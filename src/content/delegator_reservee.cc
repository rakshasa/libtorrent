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
  if (b == m_stalled || m_parent == NULL)
    return;

  m_stalled = b;

  if (b)
    m_parent->inc_stalled();
  else
    m_parent->dec_stalled();
}

}
