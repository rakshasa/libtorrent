#include "config.h"

#include <inttypes.h>

#include "algo/algo.h"
#include "torrent/exceptions.h"
#include "content/delegator_reservee.h"

#include "request_list.h"
#include "delegator.h"

using namespace algo;

namespace torrent {

RequestList::RequestList(Delegator* d, BitField* bf) :
  m_delegator(d),
  m_bitfield(bf),
  m_downloading(NULL) {

  if (m_delegator == NULL || m_bitfield == NULL)
    throw internal_error("RequestList ctor received Delegator or BitField null pointer");
}

bool
RequestList::delegate() {
  DelegatorReservee* r = m_delegator->delegate(*m_bitfield, -1);

  if (r)
    m_reservees.push_back(r);

  return r;
}

void
RequestList::cancel() {
  if (m_downloading) {
    m_downloading->set_state(DELEGATOR_NONE);

    delete m_downloading;
    m_downloading = NULL;
  }

  delete_range(m_reservees.end());
}

bool
RequestList::downloading(const Piece& p) {
  ReserveeList::iterator itr = std::find_if(m_reservees.begin(), m_reservees.end(),
					    eq(ref(p), call_member(&DelegatorReservee::get_piece)));

  if (itr == m_reservees.end())
    return false;

  if ((*m_bitfield)[(*itr)->get_piece().get_index()])
    throw internal_error("RequestList::received(...) called with a piece index we already have");

  if (m_delegator->downloading(**itr)) {
    delete_range(itr);

    m_downloading = m_reservees.front();
    m_reservees.pop_front();

    m_downloading->set_state(DELEGATOR_DOWNLOADING);

    return true;
  } else {
    return false;
  }
}

bool
RequestList::finished() {
  if (m_downloading == NULL)
    throw internal_error("RequestList::finished() called without a downloading piece");

  bool r = m_delegator->finished(*m_downloading);

  delete m_downloading;
  m_downloading = NULL;

  return r;
}


void
RequestList::delete_range(ReserveeList::iterator end) {
  while (m_reservees.begin() != end) {
    m_delegator->cancel(*m_reservees.front());
    
    delete m_reservees.front();
    m_reservees.pop_front();
  }
}

}
