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
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_DELEGATOR_RESERVEE_H
#define LIBTORRENT_DELEGATOR_RESERVEE_H

#include <inttypes.h>
#include "delegator_piece.h"

namespace torrent {

class DelegatorReservee {
public:
  ~DelegatorReservee() { invalidate(); }

  void              invalidate();

  bool              is_valid() const          { return m_parent; }
  bool              is_stalled() const        { return m_stalled; }

  void              set_stalled(bool b);

  const Piece&      get_piece() const         { return m_parent->get_piece(); }
  DelegatorPiece*   get_parent() const        { return m_parent; }

  uint32_t          get_stall_counter() const { return m_stallCounter; }

protected:
  friend class DelegatorPiece;

  DelegatorReservee(DelegatorPiece* p = NULL) : m_parent(p), m_stalled(false) {}

private:
  DelegatorReservee(const DelegatorReservee&);
  void operator = (const DelegatorReservee&);

  DelegatorPiece* m_parent;
  uint32_t        m_stallCounter;
  bool            m_stalled;
};

}

#endif
