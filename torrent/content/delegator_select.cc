#include "config.h"

#include <algo/algo.h>
#include <netinet/in.h>

#include "torrent/exceptions.h"
#include "delegator_select.h"
#include "bitfield.h"
#include "bitfield_counter.h"

using namespace algo;

namespace torrent {

void
DelegatorSelect::add_ignore(unsigned int index) {
  Indexes::iterator itr = std::find_if(m_indexes.begin(), m_indexes.end(),
				       leq(value(index), back_as_ref()));
  
  if (itr == m_indexes.end())
    throw internal_error("DelegatorSelect::add_ignore(...) received an index out of range");

  if (*itr == index)
    throw internal_error("DelegatorSelect::add_ignore(...) received an index that is already inserted");

  m_indexes.insert(itr, index);
}


void
DelegatorSelect::remove_ignore(unsigned int index) {
  Indexes::iterator itr = std::find(m_indexes.begin(), m_indexes.end(), index);
				    
  if (itr == m_indexes.end())
    throw internal_error("DelegatorSelect::remove_ignore(...) could not find the index");

  if (itr == --m_indexes.end())
    throw internal_error("DelegatorSelect::remove_ignore(...) tried to remove the last element");

  m_indexes.erase(itr);
}

int
DelegatorSelect::find(const BitField& bf, unsigned int start, unsigned int rarity) {
  int found = -1, f;
  unsigned int cur_rarity = (unsigned)-1;

  Priority::Type p = Priority::HIGH;

  while (1) {
    Priority::List::const_iterator itr = m_priority.find(p, start);

    if (itr == m_priority.get_list(p).end())
      if (p == Priority::HIGH) {
	p = Priority::NORMAL;
	continue;
      } else {
	break;
      }

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
    Priority::List::const_iterator fItr = itr;

    while (++fItr != m_priority.get_list(p).end()) {
      f = check_range(bf, fItr->first, fItr->second, rarity, cur_rarity);

      if (cur_rarity <= rarity)
	return f;
      else if (f > 0)
	found = f;
    }

    // Check ranges below the midpoint.
    Priority::List::const_reverse_iterator rItr(++itr);

    if (rItr == m_priority.get_list(p).rend())
      throw internal_error("DelegatorSelect reverse iterator borkage!?");

    while (++rItr != m_priority.get_list(p).rend()) {
      f = check_range(bf, rItr->first, rItr->second, rarity, cur_rarity);

      if (cur_rarity <= rarity)
	return f;
      else if (f > 0)
	found = f;
    }

    if (p == Priority::HIGH)
      p = Priority::NORMAL;
    else
      break;
  }

  return found;
}

int
DelegatorSelect::check_range(const BitField& bf,
			     unsigned int start,
			     unsigned int end,
			     unsigned int rarity,
			     unsigned int& cur_rarity) {

  Indexes::const_iterator indexes = std::find_if(m_indexes.begin(), m_indexes.end(),
						 leq(value(start), back_as_ref()));

  int found = -1;
  int pos = start % 8;
  
  start -= pos;

  while(start < end) {
    uint32_t v = interested(bf, start, indexes);

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
DelegatorSelect::interested(const BitField& bf,
			    unsigned int start,
			    Indexes::const_iterator& indexes) {

  uint32_t v = ntohl(*(uint32_t*)(bf.data() + start / 8) & ~*(uint32_t*)(m_bitfield->data() + start / 8));

  while (*indexes < start + 32)
    v &= ~(1 << 31 - (*(indexes++) - start));

  return v;
}    

}
