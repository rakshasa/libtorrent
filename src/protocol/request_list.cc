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

#include "config.h"

#include <inttypes.h>
#include <rak/functional.h>

#include "torrent/exceptions.h"
#include "content/delegator.h"
#include "content/delegator_reservee.h"

#include "request_list.h"

namespace torrent {

const Piece*
RequestList::delegate() {
  DelegatorReservee* r = m_delegator->delegate(*m_bitfield, m_affinity);

  if (r) {
    m_affinity = r->get_piece().get_index();
    m_reservees.push_back(r);

    return &r->get_piece();

  } else {
    return NULL;
  }
}

// Replace m_canceled with m_reservees and set them to stalled.
void
RequestList::cancel() {
  if (m_downloading)
    throw internal_error("RequestList::cancel(...) called while is_downloading() == true");

  std::for_each(m_canceled.begin(), m_canceled.end(), rak::call_delete<DelegatorReservee>());
  m_canceled.clear();

  std::for_each(m_reservees.begin(), m_reservees.end(),
		std::bind2nd(std::mem_fun(&DelegatorReservee::set_stalled), true));

  m_canceled.swap(m_reservees);
}

void
RequestList::stall() {
  std::for_each(m_reservees.begin(), m_reservees.end(),
		std::bind2nd(std::mem_fun(&DelegatorReservee::set_stalled), true));
}

bool
RequestList::downloading(const Piece& p) {
  if (m_downloading)
    throw internal_error("RequestList::downloading(...) bug, m_downloaing is already set");

  remove_invalid();

  ReserveeList::iterator itr =
    std::find_if(m_reservees.begin(), m_reservees.end(),
		 rak::equal(p, std::mem_fun(&DelegatorReservee::get_piece)));
  
  if (itr == m_reservees.end()) {
    itr = std::find_if(m_canceled.begin(), m_canceled.end(),
		       rak::equal(p, std::mem_fun(&DelegatorReservee::get_piece)));

    if (itr == m_canceled.end())
      return false;

    m_reservees.push_front(*itr);
    m_canceled.erase(itr);

  } else {
    cancel_range(itr);
  }
  
  m_downloading = true;

  if (p != m_reservees.front()->get_piece())
    throw internal_error("RequestList::downloading(...) did not add the new piece to the front of the list");
  
  if ((*m_delegator->get_select().get_bitfield())[p.get_index()])
    throw internal_error("RequestList::downloading(...) called with a piece index we already have");

  return true;
}

// Must clear the downloading piece.
void
RequestList::finished() {
  if (!m_downloading || !m_reservees.size())
    throw internal_error("RequestList::finished() called without a downloading piece");

  m_delegator->finished(*m_reservees.front());

  delete m_reservees.front();;
  m_reservees.pop_front();

  m_downloading = false;
}

void
RequestList::skip() {
  if (!m_downloading || !m_reservees.size())
    throw internal_error("RequestList::skip() called without a downloading piece");

  delete m_reservees.front();
  m_reservees.pop_front();

  m_downloading = false;
}

struct equals_reservee : public std::binary_function<DelegatorReservee*, int32_t, bool> {
  bool operator () (DelegatorReservee* r, int32_t index) const {
    return r->is_valid() && index == r->get_piece().get_index();
  }
};

bool
RequestList::has_index(uint32_t index) {
  return std::find_if(m_reservees.begin(), m_reservees.end(), std::bind2nd(equals_reservee(), index))
    != m_reservees.end();
}

void
RequestList::cancel_range(ReserveeList::iterator end) {
  while (m_reservees.begin() != end) {
    m_reservees.front()->set_stalled(true);
    
    m_canceled.push_back(m_reservees.front());
    m_reservees.pop_front();
  }
}

unsigned int
RequestList::remove_invalid() {
  uint32_t count = 0;
  ReserveeList::iterator itr;

  // Could be more efficient, but rarely do we find any.
  while ((itr = std::find_if(m_reservees.begin(), m_reservees.end(),  std::not1(std::mem_fun(&DelegatorReservee::is_valid))))
	 != m_reservees.end()) {
    count++;
    delete *itr;
    m_reservees.erase(itr);
  }

  // Don't count m_canceled that are invalid.
  while ((itr = std::find_if(m_canceled.begin(), m_canceled.end(), std::not1(std::mem_fun(&DelegatorReservee::is_valid))))
	 != m_canceled.end()) {
    delete *itr;
    m_canceled.erase(itr);
  }

  return count;
}

}
