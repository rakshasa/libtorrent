#ifndef LIBTORRENT_BITFIELD_EXT_H
#define LIBTORRENT_BITFIELD_EXT_H

#include "bitfield.h"

// Bitfield with some extra information like bit count.

namespace torrent {

class BitFieldExt : private BitField {
public:

  using BitField::size_t;
  using BitField::size_bits;
  using BitField::size_bytes;
  using BitField::get;
  using BitField::begin;
  using BitField::end;
  using BitField::operator[];

  BitFieldExt() : m_count(0) {}
  BitFieldExt(size_t s) : BitField(s), m_count(0) {}

  BitFieldExt(const BitFieldExt& bf) : BitField(bf), m_count(bf.m_count) {}

  // Call this after modifying through the begin() pointer.
  void      update_count() {
    if (this == NULL)
      throw internal_error("BitFieldExt::update_count() called on a NULL object");

    m_count = BitField::count();
  }

  size_t    count() const {
    return m_count;
  }

  bool      all_zero() const { return m_count == 0; }

  bool      all_set() const  { return m_count == size_bits(); }

  void      clear() {
    m_count = 0;
    BitField::clear();
  }

  void      set(size_t i, bool s) {
    if (get(i) == s)
      return;

    set(i, s);

    if (s)
      m_count++;
    else
      m_count--;
  }

  BitFieldExt&        operator = (const BitFieldExt& bf) {
    BitField::operator=(bf);
    return *this;
  }

  const BitField&     get_bitfield() { return *this; }

private:
  size_t m_count;
};

}

#endif
