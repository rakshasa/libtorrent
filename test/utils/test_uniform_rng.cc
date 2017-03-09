#include "stdlib.h"

#include "config.h"
#include "test_uniform_rng.h"
#include "utils/uniform_rng.h"

#define ROUNDS 1000

CPPUNIT_TEST_SUITE_REGISTRATION(TestUniformRNG);

static torrent::uniform_rng gen;

void
TestUniformRNG::test_rand() {
  for (int i = 0; i < ROUNDS; i++) {
    int num = gen.rand();
    CPPUNIT_ASSERT(0 <= num && num <= RAND_MAX);
  }
}

void
TestUniformRNG::test_rand_range() {
  for (int i = 0; i < ROUNDS; i++) {
    int num = gen.rand_range(i, 2*i);
    CPPUNIT_ASSERT(i <= num && num <= 2*i);
  }
}

void
TestUniformRNG::test_rand_below() {
  for (int i = 1; i < ROUNDS; i++) {
    int num = gen.rand_below(i);
    CPPUNIT_ASSERT(0 <= num && num < i);
  }
}

