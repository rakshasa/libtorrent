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

#ifndef LIBTORRENT_DELEGATOR_PIECE_H
#define LIBTORRENT_DELEGATOR_PIECE_H

#include <vector>
#include <inttypes.h>

#include "data/piece.h"

namespace torrent {

// Note that DelegatorReservee and DelegatorPiece do not safely
// destruct, copy and stuff like that. You need to manually make
// sure you disconnect the reservee from the piece. I don't see any
// point in making this reference counted and stuff like that since
// we have a clear place of origin and end of the objects.

class DelegatorReservee;

class DelegatorPiece {
public:
  typedef std::vector<DelegatorReservee*> Reservees;

  DelegatorPiece() : m_finished(false), m_stalled(0) {}
  ~DelegatorPiece();

  DelegatorReservee* create();

  void               clear();

  bool               is_finished()                      { return m_finished; }
  void               set_finished(bool f)               { m_finished = f; }

  const Piece&       get_piece()                        { return m_piece; }
  void               set_piece(const Piece& p)          { m_piece = p; }

  uint32_t           get_reservees_size()               { return m_reservees.size(); }
  uint16_t           get_stalled()                      { return m_stalled; }
  uint16_t           get_not_stalled()                  { return m_reservees.size() - m_stalled; }

protected:
  friend class DelegatorReservee;

  void               remove(DelegatorReservee* r);

  void               inc_stalled()                      { ++m_stalled; }
  void               dec_stalled()                      { --m_stalled; }

private:
  DelegatorPiece(const DelegatorPiece&);
  void operator = (const DelegatorPiece&);

  Piece              m_piece;
  Reservees          m_reservees;

  bool               m_finished;
  uint16_t           m_stalled;
};

}

#endif
