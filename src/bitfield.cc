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
    int a = ((m_size + 8 * sizeof(pad_type) - 1) / (8 * sizeof(pad_type))) * sizeof(pad_type);

    m_start = new char[a];
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
    m_start = new char[bf.m_pad - bf.m_start];
    m_end = m_start + (bf.m_end - bf.m_start);
    m_pad = m_start + (bf.m_pad - bf.m_start);

    std::memcpy(m_start, bf.m_start, bf.m_pad - bf.m_start);

  } else {
    m_start = m_end = m_pad = NULL;
  }
}

BitField& BitField::operator = (const BitField& bf) {
  if (this == &bf)
    return *this;

  delete [] m_start;
  
  if (bf.m_size) {
    m_size = bf.m_size;
    m_start = new char[bf.m_pad - bf.m_start];
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

BitField& BitField::not_in(const BitField& bf) {
  if (m_size != bf.m_size)
    throw internal_error("Tried to do operations between different sized bitfields");

  for (pad_type* i = (pad_type*)m_start, *i2 = (pad_type*)bf.m_start;
       i < (pad_type*)m_pad; i++, i2++)
    *i &= ~*i2;

  return *this;
}

bool BitField::all_zero() const {
  if (m_size == 0)
    return true;

  for (pad_type* i = (pad_type*)m_start; i < (pad_type*)m_pad; i++)
    if (*i)
      return false;

  return true;
}

bool BitField::all_set() const {
  if (m_size == 0)
    return false;

  char* i = m_start;
  char* end = m_pad - sizeof(pad_type);

  while (i < end) {
    if (*(pad_type*)i != (pad_type)-1)
      return false;

    i += sizeof(pad_type);
  }

  if (m_size % (8 * sizeof(pad_type)))
    return *(pad_type*)i == htonl((pad_type)-1 << 8 * sizeof(pad_type) - m_size % (8 * sizeof(pad_type)));
  else
    return *(pad_type*)i == (pad_type)-1;
}

unsigned int BitField::count() const {
  // TODO: Optimize, use a lookup table

  // for (n = 0; x; n++)
  //   x &= x-1;

  unsigned int c = 0;

  for (unsigned int i = 0; i < size_bits(); ++i)
    c += (*this)[i];

  return c;
}

}
