#include "config.h"

#include <functional>
#include <inttypes.h>

#include "algo/algo.h"
#include "torrent/exceptions.h"
#include "content/delegator.h"
#include "content/delegator_reservee.h"

#include "request_list.h"

using namespace algo;

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

  std::for_each(m_canceled.begin(), m_canceled.end(), delete_on());
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
		 eq(ref(p), call_member(&DelegatorReservee::get_piece)));
  
  if (itr == m_reservees.end()) {
    itr = std::find_if(m_canceled.begin(), m_canceled.end(),
		       eq(ref(p), call_member(&DelegatorReservee::get_piece)));

    if (itr == m_canceled.end())
      return false;

    m_reservees.push_front(*itr);
    m_canceled.erase(itr);

  } else {
    cancel_range(itr);
  }
  
  m_downloading = true;
  m_piece = m_reservees.front()->get_piece();
  
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
