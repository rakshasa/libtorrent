#ifndef LIBTORRENT_BITFIELD_H
#define LIBTORRENT_BITFIELD_H

#include <cstring>

namespace torrent {

class BitField {
public:
  BitField() :
    m_size(0),
    m_start(NULL),
    m_end(NULL),
    m_pad(NULL) {}

  BitField(unsigned int s);
  BitField(const BitField& bf);

  ~BitField() { delete [] m_start; m_start = NULL; }

  unsigned int sizeBits() const { return m_size; }
  unsigned int sizeBytes() const { return m_end - m_start; }
  unsigned int sizeInts() const { return (m_pad - m_start) / sizeof(int); }

  unsigned int count() const;

  void clear() {
    if (m_start) std::memset(m_start, 0, m_pad - m_start);
  }

  void clear(int start, int end);

  void set(unsigned int i, bool s = true) {
    m_start[i / 8] = (m_start[i / 8] & ~(1 << 7 - i % 8)) | (s % 2 << 7 - i % 8);
  }

  bool operator [] (unsigned int i) const {
    return m_start[i / 8] & (1 << 7 - i % 8);
  }
  
  BitField& operator = (const BitField& bf);

  // Mark all in this not in b2.
  BitField& notIn(const BitField& bf);

  bool zero() const;
  bool allSet() const;

  char* data() { return m_start; }
  const char* data() const { return m_start; }
  const char* end() const { return m_end; }

private:
  int m_size;

  char* m_start;
  char* m_end;
  char* m_pad;
};

}

#endif // LIBTORRENT_BITFIELD_H
