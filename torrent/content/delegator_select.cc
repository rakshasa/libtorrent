#include "config.h"

#include <algo/algo.h>
#include "exceptions.h"
#include "delegator_select.h"

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
}

}
