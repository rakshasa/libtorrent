#include "config.h"

#include <algo/algo.h>
#include <netinet/in.h>

#include "torrent/exceptions.h"
#include "delegator_select.h"
#include "utils/bitfield.h"
#include "utils/bitfield_counter.h"

using namespace algo;

namespace torrent {

bool
DelegatorSelect::interested(const BitField& bf) {
  return
    std::find_if(m_priority.begin(Priority::NORMAL), m_priority.end(Priority::NORMAL),
		      call_member(ref(*this), &DelegatorSelect::interested_range,
				  ref(bf),
				  member(&Priority::Range::first), member(&Priority::Range::second)))
    != m_priority.end(Priority::NORMAL) ||

    std::find_if(m_priority.begin(Priority::HIGH), m_priority.end(Priority::HIGH),
		      call_member(ref(*this), &DelegatorSelect::interested_range,
				  ref(bf),
				  member(&Priority::Range::first), member(&Priority::Range::second)))
    != m_priority.end(Priority::HIGH);
}

bool
DelegatorSelect::interested(uint32_t index) {
  if (index < 0 && index >= m_bitfield->size_bits())
    throw internal_error("Delegator::interested received index out of range");

  // TODO: Move the m_priorty up to peer, and make it a function here so we don't need to
  // repeat it for every peer.
  return !(*m_bitfield)[index] &&
    (m_priority.has(Priority::NORMAL, index) || m_priority.has(Priority::HIGH, index));
}

void
DelegatorSelect::add_ignore(uint32_t index) {
  Indexes::iterator itr = std::find_if(m_ignore.begin(), m_ignore.end(),
				       leq(value(index), back_as_ref()));
  
  if (itr == m_ignore.end())
    throw internal_error("DelegatorSelect::add_ignore(...) received an index out of range");

  if (*itr == index)
    throw internal_error("DelegatorSelect::add_ignore(...) received an index that is already inserted");

  m_ignore.insert(itr, index);
}


void
DelegatorSelect::remove_ignore(uint32_t index) {
  Indexes::iterator itr = std::find(m_ignore.begin(), m_ignore.end(), index);
				    
  if (itr == m_ignore.end())
    throw internal_error("DelegatorSelect::remove_ignore(...) could not find the index");

  if (itr == --m_ignore.end())
    throw internal_error("DelegatorSelect::remove_ignore(...) tried to remove the last element");

  m_ignore.erase(itr);
}

int
DelegatorSelect::find(const BitField& bf, uint32_t start, uint32_t rarity, Priority::Type p) {
  int found = -1, f;
  uint32_t cur_rarity = (unsigned)-1;

  // TODO: Ugly, refactor.
  Priority::List::iterator itr = m_priority.find(p, start);

  if (itr == m_priority.end(p))
    return found;

  // Check the range start is contained by.
  f = check_range(bf, std::max(start, itr->first), itr->second, rarity, cur_rarity);

  if (cur_rarity <= rarity)
    return f;
  else if (f > 0)
    found = f;

  f = check_range(bf, itr->first, start, rarity, cur_rarity);

  if (cur_rarity <= rarity)
    return f;
  else if (f > 0)
    found = f;

  // Check ranges above the midpoint.
  Priority::List::iterator fItr = itr;

  while (++fItr != m_priority.end(p)) {
    f = check_range(bf, fItr->first, fItr->second, rarity, cur_rarity);

    if (cur_rarity <= rarity)
      return f;
    else if (f > 0)
      found = f;
  }

  // Check ranges below the midpoint.
  Priority::List::reverse_iterator rItr(++itr);

  if (rItr == m_priority.rend(p))
    throw internal_error("DelegatorSelect reverse iterator borkage!?");

  while (++rItr != m_priority.rend(p)) {
    f = check_range(bf, rItr->first, rItr->second, rarity, cur_rarity);

    if (cur_rarity <= rarity)
      return f;
    else if (f > 0)
      found = f;
  }

  return found;
}

int32_t
DelegatorSelect::check_range(const BitField& bf,
			     uint32_t start,
			     uint32_t end,
			     uint32_t rarity,
			     uint32_t& cur_rarity) {

  Indexes::const_iterator indexes = std::find_if(m_ignore.begin(), m_ignore.end(),
						 leq(value(start), back_as_ref()));

  int32_t found = -1;
  int32_t pos = start % 8;
  
  start -= pos;

  while(start < end) {
    uint32_t v = wanted(bf, start, indexes);

    if (v) {
      while (pos < 32) {
	if (v & (1 << 31 - pos) &&
	    start + pos < end &&
	    m_seen->field()[start + pos] < cur_rarity) {

	  cur_rarity = m_seen->field()[start + pos];

	  if (cur_rarity <= rarity)
	    return start + pos;
	  else
	    found = start + pos;
	}

	++pos;
      }

      pos = 0;
    }

    start += 32;
  }

  return found;
}

// Start must lie on an 8bit boundary. Returned in network byte order.
uint32_t
DelegatorSelect::wanted(const BitField& bf,
			uint32_t start,
			Indexes::const_iterator& indexes) {

  uint32_t v = ntohl(*(uint32_t*)(bf.begin() + start / 8) & ~*(uint32_t*)(m_bitfield->begin() + start / 8));

  while (*indexes < start + 32)
    v &= ~(1 << 31 - (*(indexes++) - start));

  return v;
}    

bool
DelegatorSelect::interested_range(const BitField& bf, uint32_t start, uint32_t end) {
  unsigned char r = start % 8;

  if (r && ~(*(m_bitfield->begin() + start / 8) << 8 - r) & (*(bf.begin() + start / 8)) << 8 - r)
    return true;

  uint32_t* i1 = (uint32_t*)(m_bitfield->begin() + (start + 7) / 8);
  uint32_t* i2 = (uint32_t*)(bf.begin() + (start + 7) / 8);

  uint32_t* e1 = i1 + (end - start) / sizeof(uint32_t);

  // At this point start must be aligned to the byte.
  while (i1 != e1) {
    if (~*i1 & *i2)
      return true;

    ++i1;
    ++i2;
  }

  return (r = (end - start) / sizeof(uint32_t)) != 0 &&
    ~(ntohl(*i1) >> sizeof(uint32_t) - r) & (ntohl(*i2) >> sizeof(uint32_t) - r);
}      

}
