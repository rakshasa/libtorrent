#ifndef LIBTORRENT_BITFIELD_COUNTER_H
#define LIBTORRENT_BITFIELD_COUNTER_H

#include "bitfield.h"
#include <vector>

namespace torrent {

class BitFieldCounter {
 public:
  // Use short since we're never going to get more than 2^16 peers connected
  // at once.
  typedef std::vector<unsigned short> Field;

  BitFieldCounter() : m_field(0) {}
  BitFieldCounter(int size) : m_field(size, 0) {}

  const Field& field() const { return m_field; }

  short inc(int index) {
    return ++m_field[index];
  }

  short dec(int index) {
    return --m_field[index];
  }

  void inc(const BitField& bf) {
    if (bf.sizeBits() != m_field.size())
      throw internal_error("BitFieldCounter::inc called on fields with mismatching size");

    int a = 0;
    char* c = (char*)bf.data();

    for (Field::iterator itr = m_field.begin(); itr != m_field.end(); ++itr, ++a) {
      if (a == 8) {
	a = 0;
	++c;
      }

      if (*c & (1 << (7 - a)))
	++*itr;
    }
  }

  void dec(const BitField& bf) {
    if (bf.sizeBits() != m_field.size())
      throw internal_error("BitFieldCounter::dec called on fields with mismatching size");

    int a = 0;
    char* c = (char*)bf.data();

    for (Field::iterator itr = m_field.begin(); itr != m_field.end(); ++itr, ++a) {
      if (a == 8) {
	a = 0;
	++c;
      }

      if (*c & (1 << (7 - a)))
	--*itr;
    }
  }

 private:
  Field m_field;
};

}

#endif // LIBTORRENT_BITFIELD_COUNTER_H
