#ifndef LIBTORRENT_BITFIELD_H
#define LIBTORRENT_BITFIELD_H

#include <cstring>

namespace torrent {

class BitField {
public:
  typedef uint32_t       pad_t;
  typedef uint32_t       size_t;
  typedef uint8_t        data_t;
  typedef const uint8_t  c_data_t;

  BitField() :
    m_size(0),
    m_start(NULL),
    m_end(NULL),
    m_pad(NULL) {}

  BitField(size_t s);
  BitField(const BitField& bf);

  ~BitField() { delete [] m_start; m_start = NULL; }

  size_t    size_bits() const { return m_size; }
  size_t    size_bytes() const { return m_end - m_start; }
  size_t    size_ints() const { return (m_pad - m_start) / sizeof(pad_t); }

  // Allow this?
  size_t    count() const;

  void      clear() {
    if (m_start)
      std::memset(m_start, 0, m_pad - m_start);
  }

  void      set(size_t i, bool s) {
    if (s)
      m_start[i / 8] |= 1 << 7 - i % 8;
    else
      m_start[i / 8] &= ~(1 << 7 - i % 8);
  }

  bool      get(size_t i) const {
    return m_start[i / 8] & (1 << 7 - i % 8);
  }    

  // Mark all in this not in b2.
  BitField& not_in(const BitField& bf);

  bool      all_zero() const;
  bool      all_set() const;

  data_t*   begin() { return m_start; }
  c_data_t* begin() const { return m_start; }
  c_data_t* end() const { return m_end; }

  bool      operator [] (size_t i) const {
    return get(i);
  }
  
  BitField& operator = (const BitField& bf);

private:
  size_t    m_size;

  data_t*   m_start;
  data_t*   m_end;
  data_t*   m_pad;
};

}

#endif // LIBTORRENT_BITFIELD_H
