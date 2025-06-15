#include "config.h"

#include "bitfield.h"

#include <algorithm>

#include "exceptions.h"
#include "rak/algorithm.h"
#include "utils/instrumentation.h"

namespace torrent {

void
Bitfield::set_size_bits(size_type s) {
  if (m_data != NULL)
    throw internal_error("Bitfield::set_size_bits(size_type s) m_data != NULL.");

  m_size = s;
}

void
Bitfield::set_size_set(size_type s) {
  if (s > m_size || m_data != NULL)
    throw internal_error("Bitfield::set_size_set(size_type s) s > m_size.");

  m_set = s;
}

void
Bitfield::allocate() {
  if (m_data != nullptr)
    return;

  m_data = std::make_unique<value_type[]>(size_bytes());

  instrumentation_update(INSTRUMENTATION_MEMORY_BITFIELDS, static_cast<int64_t>(size_bytes()));
}

void
Bitfield::unallocate() {
  if (m_data == nullptr)
    return;

  m_data = nullptr;

  instrumentation_update(INSTRUMENTATION_MEMORY_BITFIELDS, -static_cast<int64_t>(size_bytes()));
}

void
Bitfield::update() {
  // Clears the unused bits.
  clear_tail();

  m_set = 0;

  iterator itr = m_data.get();
  iterator last = end();

  while (itr + sizeof(unsigned int) <= last) {
    m_set += rak::popcount_wrapper(*reinterpret_cast<unsigned int*>(itr));
    itr += sizeof(unsigned int);
  }

  while (itr != last) {
    m_set += rak::popcount_wrapper(*itr++);
  }
}

void
Bitfield::copy(const Bitfield& bf) {
  unallocate();

  m_size = bf.m_size;
  m_set = bf.m_set;

  if (bf.m_data == nullptr) {
    m_data = nullptr;
  } else {
    allocate();
    std::copy_n(bf.m_data.get(), size_bytes(), m_data.get());
  }
}

void
Bitfield::swap(Bitfield& bf) noexcept {
  std::swap(m_size, bf.m_size);
  std::swap(m_set, bf.m_set);
  std::swap(m_data, bf.m_data);
}

void
Bitfield::set_all() {
  m_set = m_size;

  std::fill_n(m_data.get(), size_bytes(), ~value_type{});
  clear_tail();
}

void
Bitfield::unset_all() {
  m_set = 0;

  std::fill_n(m_data.get(), size_bytes(), value_type{});
}

// Quick hack. Speed improvements would require that m_set is kept
// up-to-date.
void
Bitfield::set_range(size_type first, size_type last) {
  while (first != last)
    set(first++);
}

void
Bitfield::unset_range(size_type first, size_type last) {
  while (first != last)
    unset(first++);
}

} // namespace torrent
