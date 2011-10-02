#include "config.h"

#include <stdint.h>

#include "allocators_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(AllocatorsTest);

template <typename T>
bool is_aligned(const T& t) {
  return t.empty() || (reinterpret_cast<intptr_t>(&t[0]) & (LT_SMP_CACHE_BYTES - 1)) == 0x0;
}

void
AllocatorsTest::testAlignment() {
  aligned_vector_type v1;
  aligned_vector_type v2(1, 'a');
  aligned_vector_type v3(16, 'a');
  aligned_vector_type v4(LT_SMP_CACHE_BYTES, 'b');
  aligned_vector_type v5(1, 'a');
  
  CPPUNIT_ASSERT(is_aligned(v1));
  CPPUNIT_ASSERT(is_aligned(v2));
  CPPUNIT_ASSERT(is_aligned(v3));
  CPPUNIT_ASSERT(is_aligned(v4));
  CPPUNIT_ASSERT(is_aligned(v5));
}
