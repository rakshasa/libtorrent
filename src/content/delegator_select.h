#ifndef LIBTORRENT_DELEGATOR_SELECT_H
#define LIBTORRENT_DELEGATOR_SELECT_H

#include <inttypes.h>
#include "priority.h"

namespace torrent {

class BitField;
class BitFieldCounter;

class DelegatorSelect {
public:

  typedef std::vector<uint32_t> Indexes;
  
  DelegatorSelect() :
    m_bitfield(NULL),
    m_seen(NULL) {
    m_ignore.push_back((unsigned)-1);
  }
    
  bool                   interested(const BitField& bf);
  bool                   interested(uint32_t index);
  bool                   interested_range(const BitField& bf, uint32_t start, uint32_t end);

  Priority&	         get_priority()               { return m_priority; }
  const BitField*        get_bitfield()               { return m_bitfield; }
  const BitFieldCounter* get_seen()                   { return m_seen; }

  void		         set_bitfield(const BitField* bf)   { m_bitfield = bf; }
  void                   set_seen(const BitFieldCounter* s) { m_seen = s; }

  // Never touch index value 'size', this is used to avoid unnessesary
  // checks for the end iterator.
  void                   add_ignore(uint32_t index);
  void                   remove_ignore(uint32_t index);

  int                    find(const BitField& bf, uint32_t start, uint32_t rarity, Priority::Type p);

  void                   clear() {
    m_ignore.clear();
    m_priority.clear();
    m_ignore.push_back((unsigned)-1);
  }

private:
  int32_t                check_range(const BitField& bf,
				     uint32_t start,
				     uint32_t end,
				     uint32_t rarity,
				     uint32_t& cur_rarity);

  uint32_t               wanted(const BitField& bf,
				uint32_t start,
				Indexes::const_iterator& indexes);

  Indexes                m_ignore;
  Priority               m_priority;

  const BitField*        m_bitfield;
  const BitFieldCounter* m_seen;
};

}

#endif
