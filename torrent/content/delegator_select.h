#ifndef LIBTORRENT_DELEGATOR_SELECT_H
#define LIBTORRENT_DELEGATOR_SELECT_H

#include <inttypes.h>
#include "priority.h"

namespace torrent {

class BitField;
class BitFieldCounter;

class DelegatorSelect {
public:

  typedef std::vector<unsigned int> Indexes;
  
  DelegatorSelect(unsigned int size) :
    m_bitfield(NULL),
    m_seen(NULL) {
    m_indexes.push_back((unsigned)-1);
  }
    
  Priority&	   get_priority()               { return m_priority; }
  BitField*	   get_bitfield()               { return m_bitfield; }
  BitFieldCounter* get_seen()                   { return m_seen; }

  void		   set_bitfield(BitField* bf)   { m_bitfield = bf; }
  void             set_seen(BitFieldCounter* s) { m_seen = s; }

  // Never touch index value 'size', this is used to avoid unnessesary
  // checks for the end iterator.
  void             add_ignore(unsigned int index);
  void             remove_ignore(unsigned int index);

  int              find(const BitField& bf, unsigned int start, unsigned int rarity);

private:
  int              check_range(const BitField& bf,
			       unsigned int start,
			       unsigned int end,
			       unsigned int rarity,
			       unsigned int& cur_rarity);

  uint32_t         interested(const BitField& bf,
			      unsigned int start,
			      Indexes::const_iterator& indexes);

  Indexes          m_indexes;
  Priority         m_priority;

  BitField*        m_bitfield;
  BitFieldCounter* m_seen;
};

}

#endif
