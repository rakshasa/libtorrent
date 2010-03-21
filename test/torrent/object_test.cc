#include "config.h"

#include <iostream>
#include <torrent/object.h>

#import "object_test.h"
#import "object_test_utils.h"

CPPUNIT_TEST_SUITE_REGISTRATION(ObjectTest);

void object_test_return_void() { }

// template <typename Ret>
// struct object_void_wrapper {
//   typedef 

//   object_void_wrapper(Slot s) : m_slot(s) {}

//   torrent::Object operator () () { m_slot(

//   Slot m_slot;
// }

void
ObjectTest::test_basic() {
  std::cout << "sizeof(torrent::Object) = " << sizeof(torrent::Object) << std::endl;
  std::cout << "sizeof(torrent::Object::list_type) = " << sizeof(torrent::Object::list_type) << std::endl;
  std::cout << "sizeof(torrent::Object::map_type) = " << sizeof(torrent::Object::map_type) << std::endl;

//   torrent::Object obj_void(object_test_return_void());
}

void
ObjectTest::test_flags() {
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
ObjectTest::test_merge() {
}

#define TEST_VALUE_A "i10e"
#define TEST_VALUE_B "i20e"
#define TEST_STRING_A "1:g"
#define TEST_STRING_B "1:h"
#define TEST_MAP_A "d1:ai1e1:bi2ee"
#define TEST_MAP_B "d1:ci4e1:di5ee"
#define TEST_LIST_A "l1:e1:fe"
#define TEST_LIST_B "li1ei2ee"

static bool
swap_compare(const char* left, const char* right) {
  torrent::Object obj_left = create_bencode(left);
  torrent::Object obj_right = create_bencode(right);

  obj_left.swap(obj_right);
  if (!compare_bencode(obj_left, right) || !compare_bencode(obj_right, left))
    return false;

  obj_left.swap(obj_right);
  if (!compare_bencode(obj_left, left) || !compare_bencode(obj_right, right))
    return false;

  return true;
}

// static bool
// move_compare(const char* left, const char* right) {
//   torrent::Object obj_left = create_bencode(left);
//   torrent::Object obj_right = create_bencode(right);

//   obj_left.move(obj_right);
//   if (!compare_bencode(obj_left, right))
//     return false;

//   return true;
// }

void
ObjectTest::test_swap_and_move() {
  CPPUNIT_ASSERT(swap_compare(TEST_VALUE_A, TEST_VALUE_B));
  CPPUNIT_ASSERT(swap_compare(TEST_STRING_A, TEST_STRING_B));
  CPPUNIT_ASSERT(swap_compare(TEST_MAP_A, TEST_MAP_B));
  CPPUNIT_ASSERT(swap_compare(TEST_LIST_A, TEST_LIST_B));

  CPPUNIT_ASSERT(swap_compare(TEST_VALUE_A, TEST_STRING_B));
  CPPUNIT_ASSERT(swap_compare(TEST_STRING_A, TEST_MAP_B));
  CPPUNIT_ASSERT(swap_compare(TEST_MAP_A, TEST_LIST_B));
  CPPUNIT_ASSERT(swap_compare(TEST_LIST_A, TEST_VALUE_B));

  CPPUNIT_ASSERT(swap_compare("i1e", TEST_VALUE_A));
  CPPUNIT_ASSERT(swap_compare("i1e", TEST_MAP_A));
  CPPUNIT_ASSERT(swap_compare("i1e", TEST_LIST_A));
}
