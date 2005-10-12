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
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_REQUEST_LIST_H
#define LIBTORRENT_REQUEST_LIST_H

#include <deque>
#include "download/delegator_reservee.h"

namespace torrent {

class BitField;
class Delegator;

class RequestList {
public:
  typedef std::deque<DelegatorReservee*> ReserveeList;

  RequestList() :
    m_delegator(NULL),
    m_bitfield(NULL),
    m_affinity(-1),
    m_downloading(false) {}

  ~RequestList() { cancel(); }

  // Some parameters here, like how fast we are downloading and stuff
  // when we start considering those.
  const Piece*       delegate();

  // If is downloading, call skip before cancel.
  void               cancel();
  void               stall();
  void               skip();

  bool               downloading(const Piece& p);
  void               finished();

  bool               is_downloading()                 { return m_downloading; }
  bool               is_wanted()                      { return m_reservees.front()->is_valid(); }
  bool               is_interested_in_active() const;

  bool               has_index(uint32_t i);
  uint32_t           remove_invalid();

  bool               empty() const                    { return m_reservees.empty(); }
  size_t             size()                           { return m_reservees.size(); }

  Piece              get_queued_piece(uint32_t i) {
    // TODO: Make this unnessesary?
    if (m_reservees[i]->is_valid())
      return m_reservees[i]->get_piece();
    else
      return Piece();
  }

  void               set_delegator(Delegator* d)      { m_delegator = d; }
  void               set_bitfield(const BitField* b)  { m_bitfield = b; }

private:
  void               cancel_range(ReserveeList::iterator end);

  Delegator*         m_delegator;
  const BitField*    m_bitfield;

  int32_t            m_affinity;
  bool               m_downloading;

  ReserveeList       m_reservees;
  ReserveeList       m_canceled;
};

}

#endif
