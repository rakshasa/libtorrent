#ifndef LIBTORRENT_BITFIELD_H
#define LIBTORRENT_BITFIELD_H

#include <cstring>

namespace torrent {

class BitField {
public:
  typedef uint32_t     pad_type;
  typedef uint32_t     size_type;
  typedef char*        data_type;
  typedef const char*  const_data_type;

  BitField() :
    m_size(0),
    m_start(NULL),
    m_end(NULL),
    m_pad(NULL) {}

  BitField(size_type s);
  BitField(const BitField& bf);

  ~BitField() { delete [] m_start; m_start = NULL; }

  size_type           size_bits() const { return m_size; }
  size_type           size_bytes() const { return m_end - m_start; }
  size_type           size_ints() const { return (m_pad - m_start) / sizeof(pad_type); }

  size_type           count() const;

  void                clear() {
    if (m_start)
      std::memset(m_start, 0, m_pad - m_start);
  }

  //void      clear(int start, int end);

  void                set(size_type i, bool s = true) {
    if (s)
      m_start[i / 8] |= 1 << 7 - i % 8;
    else
      m_start[i / 8] &= ~(1 << 7 - i % 8);
  }

  bool                get(size_type i) const {
    return m_start[i / 8] & (1 << 7 - i % 8);
  }    

  // Mark all in this not in b2.
  BitField&           not_in(const BitField& bf);

  bool                all_zero() const;
  bool                all_set() const;

  data_type           begin() { return m_start; }
  const_data_type     begin() const { return m_start; }
  const_data_type     end() const { return m_end; }

  bool        operator [] (size_type i) const {
    return m_start[i / 8] & (1 << 7 - i % 8);
  }
  
  BitField&   operator = (const BitField& bf);

private:
  size_type   m_size;

  data_type   m_start;
  data_type   m_end;
  data_type   m_pad;
};

}

#endif // LIBTORRENT_BITFIELD_H
