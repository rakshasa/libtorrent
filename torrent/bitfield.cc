#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "bitfield.h"
#include "exceptions.h"

namespace torrent {
  
BitField::BitField(unsigned int s) :
  m_size(s) {

  if (s) {
    int a = ((m_size + 8 * sizeof(Type) - 1) / (8 * sizeof(Type))) * sizeof(Type);

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
    m_end = m_start + bf.sizeBytes();
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

BitField& BitField::notIn(const BitField& bf) {
  if (m_size != bf.m_size)
    throw internal_error("Tried to do operations between different sized bitfields");

  for (Type* i = (Type*)m_start, *i2 = (Type*)bf.m_start;
       i < (Type*)m_pad; i++, i2++)
    *i &= ~*i2;

  return *this;
}

bool BitField::zero() const {
  if (m_size == 0)
    return true;

  for (Type* i = (Type*)m_start; i < (Type*)m_pad; i++)
    if (*i)
      return false;

  return true;
}

bool BitField::allSet() const {
  if (m_size == 0)
    return false;

  char* i = m_start;
  char* end = m_pad - sizeof(Type);

  while (i < end) {
    if (*(Type*)i != (Type)-1)
      return false;

    i += sizeof(Type);
  }

  if (m_size % (8 * sizeof(Type)))
    return *(Type*)i == htonl((Type)-1 << 8 * sizeof(Type) - m_size % (8 * sizeof(Type)));
  else
    return *(Type*)i == (Type)-1;
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
