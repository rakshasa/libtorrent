#include "config.h"

#include <torrent/object.h>

#import "object_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(ObjectTest);

void
ObjectTest::testFlags() {
  torrent::Object objectFlagsValue = torrent::Object(int64_t());
  torrent::Object objectNoFlagsEmpty = torrent::Object();
  torrent::Object objectNoFlagsValue = torrent::Object(int64_t());

  objectFlagsValue.set_flags(torrent::Object::flag_static_data | torrent::Object::flag_session_data);

  CPPUNIT_ASSERT(objectNoFlagsEmpty.flags() == 0);
  CPPUNIT_ASSERT(objectNoFlagsValue.flags() == 0);
  CPPUNIT_ASSERT(objectFlagsValue.flags() & torrent::Object::flag_session_data &&
                 objectFlagsValue.flags() & torrent::Object::flag_static_data);

  objectFlagsValue.unset_flags(torrent::Object::flag_session_data);

  CPPUNIT_ASSERT(!(objectFlagsValue.flags() & torrent::Object::flag_session_data) &&
                 objectFlagsValue.flags() & torrent::Object::flag_static_data);
}

void
ObjectTest::testMerge() {
}
