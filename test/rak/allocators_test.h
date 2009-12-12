#include <cppunit/extensions/HelperMacros.h>

#include <vector>

#include "rak/allocators.h"

class AllocatorsTest : public CppUnit::TestFixture {
  CPPUNIT_TEST_SUITE(AllocatorsTest);
  CPPUNIT_TEST(testAlignment);
  CPPUNIT_TEST_SUITE_END();

public:
  typedef std::vector<char, rak::cacheline_allocator<char> > aligned_vector_type;

  void setUp() {}
  void tearDown() {}

  void testAlignment();
};
