#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "bitfield.h"
#include "torrent/exceptions.h"

namespace torrent {
  
BitField::BitField(unsigned int s) :
  m_size(s) {

  if (s) {
    int a = (m_size + 7) / 8 + sizeof(pad_t);

    m_start = new data_t[a];
    m_end = m_start + (m_size + 7) / 8;
    m_pad = m_start + a;

  } else {
    m_start = m_end = m_pad = NULL;
  }

  clear();
}

BitField::BitField(const BitField& bf) :
  m_size(bf.m_size) {

  if (m_size) {
    m_start = new data_t[bf.m_pad - bf.m_start];
    m_end = m_start + (bf.m_end - bf.m_start);
    m_pad = m_start + (bf.m_pad - bf.m_start);

    std::memcpy(m_start, bf.m_start, bf.m_pad - bf.m_start);

  } else {
    m_start = m_end = m_pad = NULL;
  }
}

BitField&
BitField::operator = (const BitField& bf) {
  if (this == &bf)
    return *this;

  delete [] m_start;
  
  if (bf.m_size) {
    m_size = bf.m_size;

    m_start = new data_t[bf.m_pad - bf.m_start];
    m_end = m_start + bf.size_bytes();
    m_pad = m_start + (bf.m_pad - bf.m_start);
    
    std::memcpy(m_start, bf.m_start, m_pad - m_start);
    
  } else {
    m_size = 0;
    m_start = NULL;
    m_end = NULL;
    m_pad = NULL;
  }
  
  return *this;
}

BitField&
BitField::not_in(const BitField& bf) {
  if (m_size != bf.m_size)
    throw internal_error("Tried to do operations between different sized bitfields");

  for (pad_t* i = (pad_t*)m_start, *i2 = (pad_t*)bf.m_start;
       i < (pad_t*)m_pad; i++, i2++)
    *i &= ~*i2;

  return *this;
}

bool
BitField::all_zero() const {
  if (m_size == 0)
    return true;

  for (pad_t* i = (pad_t*)m_start; i < (pad_t*)m_pad; i++)
    if (*i)
      return false;

  return true;
}

bool
BitField::all_set() const {
  if (m_size == 0)
    return false;

  data_t* i = m_start;
  data_t* end = m_pad - sizeof(pad_t);

  while (i < end) {
    if (*(pad_t*)i != ~pad_t())
      return false;

    i += sizeof(pad_t);
  }

  if (m_size % (8 * sizeof(pad_t)))
    return *(pad_t*)i == htonl(~pad_t() << 8 * sizeof(pad_t) - m_size % (8 * sizeof(pad_t)));
  else
    return *(pad_t*)i == ~pad_t();
}

static const unsigned char bit_count_256[] = 
{
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 
  4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

uint32_t
BitField::count() const {
  // Might be faster since it doesn't touch the memory.
  // for (n = 0; x; n++)
  //   x &= x-1;

  size_t c = 0;

  for (data_t* i = m_start; i != m_end; ++i)
    c += bit_count_256[*i];

  return c;
}

void
BitField::set(size_t begin, size_t end, bool s) {
  // Quick, dirty and slow.
  while (begin != end)
    set(begin++, s);
}

}
