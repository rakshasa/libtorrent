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
  
  DelegatorSelect() :
    m_bitfield(NULL),
    m_seen(NULL) {
    m_ignore.push_back((unsigned)-1);
  }
    
  Priority&	         get_priority()               { return m_priority; }
  const BitField*        get_bitfield()               { return m_bitfield; }
  const BitFieldCounter* get_seen()                   { return m_seen; }

  void		         set_bitfield(const BitField* bf)   { m_bitfield = bf; }
  void                   set_seen(const BitFieldCounter* s) { m_seen = s; }

  // Never touch index value 'size', this is used to avoid unnessesary
  // checks for the end iterator.
  void                   add_ignore(unsigned int index);
  void                   remove_ignore(unsigned int index);

  int                    find(const BitField& bf, unsigned int start, unsigned int rarity);

private:
  int                    check_range(const BitField& bf,
				     unsigned int start,
				     unsigned int end,
				     unsigned int rarity,
				     unsigned int& cur_rarity);

  uint32_t               interested(const BitField& bf,
				    unsigned int start,
				    Indexes::const_iterator& indexes);

  Indexes                m_ignore;
  Priority               m_priority;

  const BitField*        m_bitfield;
  const BitFieldCounter* m_seen;
};

}

#endif
