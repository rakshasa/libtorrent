#include "config.h"

#include <iostream>
#include <torrent/object.h>

#include "object_test.h"
#include "object_test_utils.h"

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
  // std::cout << "sizeof(torrent::Object) = " << sizeof(torrent::Object) << std::endl;
  // std::cout << "sizeof(torrent::Object::list_type) = " << sizeof(torrent::Object::list_type) << std::endl;
  // std::cout << "sizeof(torrent::Object::map_type) = " << sizeof(torrent::Object::map_type) << std::endl;

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

static bool
swap_compare_dict_key(const char* left_key, const char* left_obj, const char* right_key, const char* right_obj) {
  torrent::Object obj_left = torrent::Object::create_dict_key();
  torrent::Object obj_right = torrent::Object::create_dict_key();

  obj_left.as_dict_key()  = left_key;
  obj_left.as_dict_obj()  = create_bencode(left_obj);
  obj_right.as_dict_key() = right_key;
  obj_right.as_dict_obj() = create_bencode(right_obj);

  obj_left.swap(obj_right);
  if (obj_left.as_dict_key() != right_key || !compare_bencode(obj_left.as_dict_obj(), right_obj) ||
      obj_right.as_dict_key() != left_key || !compare_bencode(obj_right.as_dict_obj(), left_obj))
    return false;

  obj_left.swap(obj_right);
  if (obj_left.as_dict_key() != left_key || !compare_bencode(obj_left.as_dict_obj(), left_obj) ||
      obj_right.as_dict_key() != right_key || !compare_bencode(obj_right.as_dict_obj(), right_obj))
    return false;

  return true;
}

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

  CPPUNIT_ASSERT(swap_compare_dict_key("a", TEST_VALUE_A, "b", TEST_STRING_B));
  CPPUNIT_ASSERT(swap_compare_dict_key("a", TEST_STRING_A, "b", TEST_STRING_B));
}

void
ObjectTest::test_create_normal() {
  torrent::Object obj;

  CPPUNIT_ASSERT(torrent::object_create_normal(create_bencode_raw_bencode_c("i45e")).as_value() == 45);
  CPPUNIT_ASSERT(torrent::object_create_normal(create_bencode_raw_bencode_c("4:test")).as_string() == "test");
  CPPUNIT_ASSERT(torrent::object_create_normal(create_bencode_raw_bencode_c("li5ee")).as_list().front().as_value() == 5);
  CPPUNIT_ASSERT(torrent::object_create_normal(create_bencode_raw_bencode_c("d1:ai6ee")).as_map()["a"].as_value() == 6);

  CPPUNIT_ASSERT(torrent::object_create_normal(create_bencode_raw_string_c("test")).as_string() == "test");
  CPPUNIT_ASSERT(torrent::object_create_normal(create_bencode_raw_list_c("i5ei6e")).as_list().back().as_value() == 6);
  CPPUNIT_ASSERT(torrent::object_create_normal(create_bencode_raw_map_c("1:ai2e1:bi3e")).as_map()["b"].as_value() == 3);
}
