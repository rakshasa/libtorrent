#include "config.h"

#include "torrent/exceptions.h"
#include "delegator_piece.h"
#include "delegator_reservee.h"

namespace torrent {

void
DelegatorPiece::clear() {
  if (m_reservee)
    m_reservee->set_parent(NULL);

  m_reservee = NULL;
}

}
