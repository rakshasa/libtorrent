#include "config.h"

#include <algo/algo.h>
#include <netinet/in.h>

#include "exceptions.h"
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
  int found = -1;
  unsigned int i = start;
  unsigned int cur_rarity = (unsigned)-1;

  Priority::Type p = Priority::NORMAL;
  Priority::List::const_iterator itr = m_priority.get_list(p).begin();

  if (itr == m_priority.get_list(p).end())
    throw internal_error("DelegatorSelect::find(...) got bad priority object");

  // Todo: Add priorities loop here after doing some testing on the rest.
  found = check_range(bf, start, itr->second, rarity, cur_rarity);
  
  if (found > 0)
    return found;

  return check_range(bf, 0, start, rarity, cur_rarity);
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

  do {
    uint32_t v = ntohl(interested(bf, start, indexes));

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

  } while((start += 32) < end);

  return found;
}

// Start must lie on an 8bit boundary.
uint32_t
DelegatorSelect::interested(const BitField& bf,
			    unsigned int start,
			    Indexes::const_iterator& indexes) {

  uint32_t v = *(uint32_t*)(bf.data() + start / 8) & ~*(uint32_t*)(m_bitfield->data() + start / 8);

  while (*indexes < start + 32)
    v &= ~(1 << *(++indexes) - start);

  return v;
}    

}
