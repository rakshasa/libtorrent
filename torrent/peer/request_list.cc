#include "config.h"

#include <inttypes.h>

#include "algo/algo.h"
#include "torrent/exceptions.h"
#include "content/delegator_reservee.h"

#include "request_list.h"
#include "delegator.h"

using namespace algo;

namespace torrent {

const Piece*
RequestList::delegate() {
  DelegatorReservee* r = m_delegator->delegate(*m_bitfield, -1);

  if (r) {
    m_reservees.push_back(r);
    r->set_state(DELEGATOR_QUEUED);

    return &r->get_piece();

  } else {
    return NULL;
  }
}

void
RequestList::cancel() {
  m_downloading = false;

  cancel_range(m_reservees.end());
}

void
RequestList::stall() {
  cancel();
}

bool
RequestList::downloading(const Piece& p) {
  if (m_reservees.size() && m_reservees.front()->get_state() == DELEGATOR_DOWNLOADING)
    throw internal_error("RequestList::downloading(...) called but m_reservees.front() is in state DELEGATOR_DOWNLOADING");

  if (m_downloading)
    throw internal_error("RequestList::downloading(...) bug, m_downloaing is already set");

  ReserveeList::iterator itr = std::find_if(m_reservees.begin(), m_reservees.end(),
					    eq(ref(p), call_member(&DelegatorReservee::get_piece)));

  if (itr == m_reservees.end())
    return false;

  if ((*m_delegator->select().get_bitfield())[p.get_index()])
    throw internal_error("RequestList::downloading(...) called with a piece index we already have");

  if (m_delegator->downloading(**itr)) {
    cancel_range(itr);

    m_downloading = true;
    //m_reservees.front()->set_state(DELEGATOR_DOWNLOADING);

    return true;
  } else {
    // TODO: Do something here about the queue.
    return false;
  }
}

// Must clear the downloading piece.
bool
RequestList::finished() {
  if (!m_downloading || !m_reservees.size())
    throw internal_error("RequestList::finished() called without a downloading piece");

  bool r = m_delegator->finished(*m_reservees.front());

  delete m_reservees.front();;
  m_reservees.pop_front();

  m_downloading = false;

  return r;
}

bool
RequestList::has_index(unsigned int i) {
  return std::find_if(m_reservees.begin(), m_reservees.end(),
		      eq(value((signed int)i), call_member(call_member(&DelegatorReservee::get_piece), &Piece::get_index)))
    != m_reservees.end();
}

void
RequestList::cancel_range(ReserveeList::iterator end) {
  while (m_reservees.begin() != end) {
    m_delegator->cancel(*m_reservees.front());
    
    delete m_reservees.front();
    m_reservees.pop_front();
  }
}

}
