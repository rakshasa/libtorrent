#include "config.h"

#include "delegator_piece.h"
#include "delegator_reservee.h"

namespace torrent {

void
DelegatorPiece::clear() {
  if (m_state >= QUEUED)
    ;// Do clearing stuff here
}

}
