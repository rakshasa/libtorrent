#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bitfield.h"
#include "exceptions.h"

namespace torrent {

BitField& BitField::operator = (const BitField& bf) {
  if (this == &bf)
    return *this;

  delete [] m_start;
  
  if (bf.m_size) {
    m_size = bf.m_size;
    m_start = new int[sizeInts()];
    m_end = (char*)m_start + bf.sizeBytes();
    
    if (m_start)
      std::memcpy(m_start, bf.m_start, bf.sizeInts() * sizeof(int));
    
  } else {
    m_size = 0;
    m_start = NULL;
    m_end = NULL;
  }
  
  return *this;
}

BitField& BitField::notIn(const BitField& bf) {
  if (m_size != bf.m_size)
    throw internal_error("Tried to do operations between different sized bitfields");

  for (int* i = m_start, *i2 = bf.m_start;
       i < (int*)m_end; i++, i2++)
    *i &= ~*i2;

  return *this;
}

bool BitField::zero() const {
  if (m_size == 0)
    return true;

  for (int* i = (int*)m_start; i < (int*)m_end; i++)
    if (*i)
      return false;

  return true;
}

bool BitField::allSet() const {
  if (m_size == 0)
    return false;

  int* i;

  for (i = m_start; i + 1 < (int*)m_end; ++i)
    if (*i != -1)
      return false;

  for (; (char*)i + 1 < m_end; ++(char*)i)
    if (*(char*)i != -1)
      return false;

  if (m_size % 8)
    return -1 << (8 - m_size % 8) == *(char*)i;
  else
    return *(char*)i == -1;
}

unsigned int BitField::count() const {
  // TODO: Optimize, use a lookup table

  unsigned int c = 0;

  for (unsigned int i = 0; i < sizeBits(); ++i)
    c += (*this)[i];

  return c;
}

void BitField::clear(int start, int end) {
  // TODO, optimize

  while (start != end) {
    set(start, false);

    ++start;
  }
}

}

