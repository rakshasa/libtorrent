#ifndef LIBTORRENT_BITFIELD_H
#define LIBTORRENT_BITFIELD_H

#include <cstring>

namespace torrent {

class BitField {
public:
  typedef uint32_t       size_t;
  typedef uint8_t        data_t;
  typedef const uint8_t  c_data_t;

  BitField() :
    m_size(0),
    m_start(NULL),
    m_end(NULL) {}

  BitField(size_t s);
  BitField(const BitField& bf);

  ~BitField()                             { delete [] m_start; m_start = NULL; }

  void      clear()                       { if (m_start) std::memset(m_start, 0, size_bytes()); }

  size_t    size_bits() const             { return m_size; }
  size_t    size_bytes() const            { return m_end - m_start; }

  // Allow this?
  size_t    count() const;

  // Clear the last byte's padding.
  void      cleanup() {
    if (m_start && m_size % 8)
      *(m_end - 1) &= ~(data_t)0 << 8 - m_size % 8;
  }

  void      set(size_t i, bool s) {
    if (s)
      m_start[i / 8] |= 1 << 7 - i % 8;
    else
      m_start[i / 8] &= ~(1 << 7 - i % 8);
  }

  void      set(size_t begin, size_t end, bool s);

  bool      get(size_t i) const           { return m_start[i / 8] & (1 << 7 - i % 8); }    

  // Mark all in this not in b2.
  BitField& not_in(const BitField& bf);

  bool      all_zero() const;
  bool      all_set() const;

  data_t*   begin()                       { return m_start; }
  c_data_t* begin() const                 { return m_start; }
  c_data_t* end() const                   { return m_end; }

  bool      operator [] (size_t i) const  { return get(i); }
  
  BitField& operator = (const BitField& bf);

private:
  size_t    m_size;

  data_t*   m_start;
  data_t*   m_end;
};

}

#endif // LIBTORRENT_BITFIELD_H
