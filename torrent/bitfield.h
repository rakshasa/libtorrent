#ifndef LIBTORRENT_BITFIELD_H
#define LIBTORRENT_BITFIELD_H

#include <vector>

namespace torrent {

class BitField {
public:
  BitField() : m_size(0), m_start(NULL), m_end(NULL) {}

  BitField(unsigned int s) : m_size(s) {
    m_start = s ? new int[sizeInts()] : NULL;
    m_end = (char*)m_start + (s + 7) / 8;

    clear();
  }

  BitField(const BitField& bf) :
    m_size(bf.m_size),
    m_start(bf.m_size ? new int[bf.sizeInts()] : NULL) {

    m_end = (char*)m_start + bf.sizeBytes();

    if (m_start)
      std::memcpy(m_start, bf.m_start, bf.sizeInts() * sizeof(int));
  }

  ~BitField() { delete [] m_start; m_start = NULL; }

  unsigned int sizeBits() const { return m_size; }
  unsigned int sizeBytes() const { return m_end - (char*)m_start; }
  unsigned int sizeInts() const { return (m_size + 8 * sizeof(int) - 1) / (8 * sizeof(int)); }

  unsigned int count() const;

  void clear() {
    if (m_start) std::memset(m_start, 0, sizeInts() * sizeof(int));
  }

  void clear(int start, int end);

  void set(unsigned int i, bool s = true) {
    ((char*)m_start)[i / 8] =
      (((char*)m_start)[i / 8] & ~((1 << 7) >> (i % 8)))
      + (s << (7 - i % 8));
  }

  bool operator [] (unsigned int i) const {
    return ((char*)m_start)[i / 8] & ((1 << 7) >> (i % 8));
  }
  
  BitField& operator = (const BitField& bf);

  // Mark all in this not in b2.
  BitField& notIn(const BitField& bf);

  bool zero() const;
  bool allSet() const;

  char* data() { return (char*)m_start; }
  const char* data() const { return (char*)m_start; }
  const char* end() const { return (char*)m_end; }

private:
  int m_size;
  int* m_start;
  char* m_end;
};

}

#endif // LIBTORRENT_BITFIELD_H
