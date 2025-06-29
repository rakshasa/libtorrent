#ifndef LIBTORRENT_BITFIELD_H
#define LIBTORRENT_BITFIELD_H

#include <cstring>
#include <torrent/common.h>

namespace torrent {

class LIBTORRENT_EXPORT Bitfield {
public:
  using size_type        = uint32_t;
  using value_type       = uint8_t;
  using const_value_type = const uint8_t;
  using iterator         = value_type*;
  using const_iterator   = const value_type*;

  Bitfield() = default;
  ~Bitfield()                                       { clear(); }
  Bitfield(const Bitfield&) = delete;
  Bitfield& operator=(const Bitfield&) = delete;

  bool                empty() const                 { return m_data == nullptr; }

  bool                is_all_set() const            { return m_set == m_size; }
  bool                is_all_unset() const          { return m_set == 0; }

  bool                is_tail_cleared() const       { return m_size % 8 == 0 || !((*(end() - 1) & mask_from(m_size % 8))); }

  size_type           size_bits() const             { return m_size; }
  size_type           size_bytes() const            { return (m_size + 7) / 8; }

  size_type           size_set() const              { return m_set; }
  size_type           size_unset() const            { return m_size - m_set; }

  void                set_size_bits(size_type s);
  void                set_size_set(size_type s);

  // Call update if you've changed the data directly and want to
  // update the counters and unset the last unused bits.
  //
  // Resize clears the data?
  void                update();

  void                allocate();
  void                unallocate();

  void                clear()                       { unallocate(); m_size = 0; m_set = 0; }
  void                clear_tail()                  { if (m_size % 8) *(end() - 1) &= mask_before(m_size % 8); }

  void                copy(const Bitfield& bf);
  void                swap(Bitfield& bf) noexcept;

  void                set_all();
  void                set_range(size_type first, size_type last);

  void                unset_all();
  void                unset_range(size_type first, size_type last);

  bool                get(size_type idx) const      { return m_data[idx / 8] & mask_at(idx % 8); }

  void                set(size_type idx)            { m_set += !get(idx); m_data[idx / 8] |=  mask_at(idx % 8); }
  void                unset(size_type idx)          { m_set -=  get(idx); m_data[idx / 8] &= ~mask_at(idx % 8); }

  iterator            begin()                       { return m_data.get(); }
  const_iterator      begin() const                 { return m_data.get(); }
  iterator            end()                         { return m_data.get() + size_bytes(); }
  const_iterator      end() const                   { return m_data.get() + size_bytes(); }

  size_type           position(const_iterator itr) const  { return (itr - m_data.get()) * 8; }

  void                from_c_str(const char* str)   { std::memcpy(m_data.get(), str, size_bytes()); update(); }

  // Remember to use modulo.
  static value_type   mask_at(size_type idx)        { return 1 << (7 - idx); }
  static value_type   mask_before(size_type idx)    { return value_type{0xff} << (8 - idx); }
  static value_type   mask_from(size_type idx)      { return value_type{0xff} >> idx; }

private:
  size_type           m_size{};
  size_type           m_set{};

  std::unique_ptr<value_type[]> m_data;
};

} // namespace torrent

#endif
