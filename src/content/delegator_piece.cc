#include "config.h"

#include "torrent/exceptions.h"
#include "delegator_piece.h"
#include "delegator_reservee.h"

namespace torrent {

DelegatorPiece::~DelegatorPiece() {
  if (m_reservee)
    throw internal_error("DelegatorPiece dtor called on an object that still has a reservee");
}

}
