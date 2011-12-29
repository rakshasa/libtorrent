#include "config.h"

#include <torrent/object.h>
#include <torrent/object_stream.h>
#include "torrent/object_static_map.h"
#include "protocol/extensions.h"

#include "object_test_utils.h"
#include "object_static_map_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(ObjectStaticMapTest);

// Possible bencode keys in a DHT message.
enum keys {
  key_d_a,
  key_d_d_a,
  key_d_b,

  key_e_0,
  key_e_1,
  key_e_2,

  key_s_a,
  key_s_b,
  key_v_a,
  key_v_b,

  key_LAST,
};

enum keys_2 {
  key_2_d_x_y,
  key_2_d_x_z,
  key_2_l_x_1,
  key_2_l_x_2,
  key_2_s_a,
  key_2_v_a,
  key_2_LAST
};

typedef torrent::static_map_type<keys, key_LAST> test_map_type;
typedef torrent::static_map_type<keys_2, key_2_LAST> test_map_2_type;

// List of all possible keys we need/support in a DHT message.
// Unsupported keys we receive are dropped (ignored) while decoding.
// See torrent/object_static_map.h for how this works.
template <>
const test_map_type::key_list_type test_map_type::keys = {
  { key_d_a,          "d_a::b" },
  { key_d_d_a,        "d_a::c::a" },
  { key_d_b,          "d_a::d" },

  { key_e_0,          "e[]" },
  { key_e_1,          "e[]" },
  { key_e_2,          "e[]" },

  { key_s_a,          "s_a" },
  { key_s_b,          "s_b" },

  { key_v_a,          "v_a" },
  { key_v_b,          "v_b" },
};

template <>
const test_map_2_type::key_list_type test_map_2_type::keys = {
  { key_2_d_x_y,        "d_x::f" },
  { key_2_d_x_z,        "d_x::g" },
  { key_2_l_x_1,        "l_x[]" },
  { key_2_l_x_2,        "l_x[]" },
  { key_2_s_a,          "s_a" },
  { key_2_v_a,          "v_a" },
};

void
ObjectStaticMapTest::test_basics() {
  test_map_type test_map;

  test_map[key_v_a] = int64_t(1);
  test_map[key_v_b] = int64_t(2);
 
  test_map[key_s_a] = std::string("a");
  test_map[key_s_b] = std::string("b");

  CPPUNIT_ASSERT(test_map[key_v_a].as_value() == 1);
  CPPUNIT_ASSERT(test_map[key_v_b].as_value() == 2);
  CPPUNIT_ASSERT(test_map[key_s_a].as_string() == "a");
  CPPUNIT_ASSERT(test_map[key_s_b].as_string() == "b");
}

void
ObjectStaticMapTest::test_write() {
  test_map_type test_map;

  test_map[key_v_a] = int64_t(1);
  test_map[key_v_b] = int64_t(2);
 
  test_map[key_s_a] = std::string("a");
  //  test_map[key_s_b] = std::string("b");

  test_map[key_d_a] = std::string("xx");
  test_map[key_d_d_a] = std::string("a");
  test_map[key_d_b] = std::string("yy");

  test_map[key_e_0] = std::string("f");
  test_map[key_e_1] = torrent::object_create_raw_bencode_c_str("1:g");
  test_map[key_e_2] = std::string("h");

  char buffer[1024];

  torrent::object_buffer_t result = torrent::static_map_write_bencode_c(torrent::object_write_to_buffer,
                                                                       NULL,
                                                                       std::make_pair(buffer, buffer + sizeof(buffer)),
                                                                       test_map);

//   std::cout << "static map write: '" << std::string(buffer, std::distance(buffer, result.first)) << "'" << std::endl;

  CPPUNIT_ASSERT(validate_bencode(buffer, result.first));
}

static const char* test_read_bencode = "d3:d_xd1:fi3e1:gli4ei5eee3:l_xli1ei2ei3ee3:s_a1:a3:v_ai2ee";
static const char* test_read_skip_bencode = "d3:d_xd1:fi3e1:gli4ei5eee1:fi1e3:l_xli1ei2ei3ee3:s_a1:a3:v_ai2ee";

template <typename map_type>
bool static_map_read_bencode(map_type& map, const char* str) {
  try {
    return torrent::static_map_read_bencode(str, str + strlen(str), map) == str + strlen(str);
  } catch (torrent::bencode_error&) { return false; }
}

template <typename map_type>
bool static_map_read_bencode_exception(map_type& map, const char* str) {
  try {
    torrent::static_map_read_bencode(str, str + strlen(str), map);
    return false;
  } catch (torrent::bencode_error&) { return true; }
}

void
ObjectStaticMapTest::test_read() {
  test_map_2_type test_map;

  CPPUNIT_ASSERT(static_map_read_bencode(test_map, test_read_bencode));
  CPPUNIT_ASSERT(test_map[key_2_d_x_y].as_value() == 3);
  CPPUNIT_ASSERT(test_map[key_2_d_x_z].as_list().size() == 2);
  CPPUNIT_ASSERT(test_map[key_2_l_x_1].as_value() == 1);
  CPPUNIT_ASSERT(test_map[key_2_l_x_2].as_value() == 2);
  CPPUNIT_ASSERT(test_map[key_2_s_a].as_string() == "a");
  CPPUNIT_ASSERT(test_map[key_2_v_a].as_value() == 2);

  test_map_2_type test_skip_map;

   CPPUNIT_ASSERT(static_map_read_bencode(test_skip_map, test_read_skip_bencode));
}

enum ext_test_keys {
  key_e,
  // key_m_utMetadata,
  key_m_utPex,
  // key_metadataSize,
  key_p,
  key_reqq,
  key_v,
  key_test_LAST
};

typedef torrent::static_map_type<ext_test_keys, key_test_LAST> ext_test_message;

template <>
const ext_test_message::key_list_type ext_test_message::keys = {
  { key_e,            "e" },
  { key_m_utPex,      "m::ut_pex" },
  { key_p,            "p" },
  { key_reqq,         "reqq" },
  { key_v,            "v" },
};

void
ObjectStaticMapTest::test_read_extensions() {
  ext_test_message test_ext;

  CPPUNIT_ASSERT(static_map_read_bencode(test_ext, "d1:ai1ee"));

  CPPUNIT_ASSERT(static_map_read_bencode(test_ext, "d1:mi1ee"));
  CPPUNIT_ASSERT(static_map_read_bencode(test_ext, "d1:mdee"));
  CPPUNIT_ASSERT(static_map_read_bencode(test_ext, "d6:ut_pexi0ee"));
  CPPUNIT_ASSERT(static_map_read_bencode(test_ext, "d1:md6:ut_pexi0eee"));
}

template <typename map_type>
bool static_map_write_bencode(map_type map, const char* original) {
  try {
    char buffer[1023];
    char* last = torrent::static_map_write_bencode_c(&torrent::object_write_to_buffer,
                                                    NULL,
                                                    torrent::object_buffer_t(buffer, buffer + 1024),
                                                    map).first;
    return std::strncmp(buffer, original, std::distance(buffer, last)) == 0;

  } catch (torrent::bencode_error& e) {
    return false;
  }
}

//
// Proper unit tests:
//

enum keys_empty { key_empty_LAST };
enum keys_single { key_single_a, key_single_LAST };
enum keys_raw { key_raw_a, key_raw_LAST };
enum keys_raw_types { key_raw_types_empty, key_raw_types_list, key_raw_types_map, key_raw_types_str, key_raw_types_LAST};
enum keys_multiple { key_multiple_a, key_multiple_b, key_multiple_c, key_multiple_LAST };
enum keys_dict { key_dict_a_b, key_dict_LAST };

typedef torrent::static_map_type<keys_empty, key_empty_LAST> test_empty_type;
typedef torrent::static_map_type<keys_single, key_single_LAST> test_single_type;
typedef torrent::static_map_type<keys_raw, key_raw_LAST> test_raw_type;
typedef torrent::static_map_type<keys_raw_types, key_raw_types_LAST> test_raw_types_type;
typedef torrent::static_map_type<keys_multiple, key_multiple_LAST> test_multiple_type;
typedef torrent::static_map_type<keys_dict, key_dict_LAST> test_dict_type;

template <> const test_empty_type::key_list_type
test_empty_type::keys = { };
template <> const test_single_type::key_list_type
test_single_type::keys = { { key_single_a, "b" } };
template <> const test_raw_type::key_list_type
test_raw_type::keys = { { key_raw_a, "b*" } };
template <> const test_raw_types_type::key_list_type
test_raw_types_type::keys = { { key_raw_types_empty, "e*"},
                              { key_raw_types_list, "l*L"},
                              { key_raw_types_map, "m*M"},
                              { key_raw_types_str, "s*S"} };
template <> const test_multiple_type::key_list_type
test_multiple_type::keys = { { key_multiple_a, "a" }, { key_multiple_b, "b*" }, { key_multiple_c, "c" } };
template <> const test_dict_type::key_list_type
test_dict_type::keys = { { key_dict_a_b, "a::b" } };

void
ObjectStaticMapTest::test_read_empty() {
  test_empty_type map_normal;
  test_empty_type map_skip;
  test_empty_type map_incomplete;

  CPPUNIT_ASSERT(static_map_read_bencode(map_normal, "de"));
  CPPUNIT_ASSERT(static_map_read_bencode(map_skip, "d1:ai1ee"));
  
  CPPUNIT_ASSERT(static_map_read_bencode_exception(map_incomplete, ""));
  CPPUNIT_ASSERT(static_map_read_bencode_exception(map_incomplete, "d"));
}

void
ObjectStaticMapTest::test_read_single() {
  test_single_type map_normal;
  test_single_type map_skip;
  test_single_type map_incomplete;

  CPPUNIT_ASSERT(static_map_read_bencode(map_normal, "d1:bi1ee"));
  CPPUNIT_ASSERT(map_normal[key_single_a].as_value() == 1);

  CPPUNIT_ASSERT(static_map_read_bencode(map_skip, "d1:ai0e1:bi1e1:ci2ee"));
  CPPUNIT_ASSERT(map_skip[key_single_a].as_value() == 1);

  CPPUNIT_ASSERT(static_map_read_bencode_exception(map_incomplete, "d"));
  CPPUNIT_ASSERT(static_map_read_bencode_exception(map_incomplete, "d1:b"));
  CPPUNIT_ASSERT(static_map_read_bencode_exception(map_incomplete, "d1:bi1"));
  CPPUNIT_ASSERT(static_map_read_bencode_exception(map_incomplete, "d1:bi1e"));
}

void
ObjectStaticMapTest::test_read_single_raw() {
  test_raw_type map_raw;

  CPPUNIT_ASSERT(static_map_read_bencode(map_raw, "d1:bi1ee"));
  CPPUNIT_ASSERT(torrent::raw_bencode_equal_c_str(map_raw[key_raw_a].as_raw_bencode(), "i1e"));

  CPPUNIT_ASSERT(static_map_read_bencode(map_raw, "d1:b2:abe"));
  CPPUNIT_ASSERT(torrent::raw_bencode_equal_c_str(map_raw[key_raw_a].as_raw_bencode(), "2:ab"));
  CPPUNIT_ASSERT(torrent::raw_bencode_equal_c_str(map_raw[key_raw_a].as_raw_bencode().as_raw_string(), "ab"));

  CPPUNIT_ASSERT(static_map_read_bencode(map_raw, "d1:bli1ei2eee"));
  CPPUNIT_ASSERT(torrent::raw_bencode_equal_c_str(map_raw[key_raw_a].as_raw_bencode(), "li1ei2ee"));
  CPPUNIT_ASSERT(torrent::raw_bencode_equal_c_str(map_raw[key_raw_a].as_raw_bencode().as_raw_list(), "i1ei2e"));

  CPPUNIT_ASSERT(static_map_read_bencode(map_raw, "d1:bd1:ai1e1:bi2eee"));
  CPPUNIT_ASSERT(torrent::raw_bencode_equal_c_str(map_raw[key_raw_a].as_raw_bencode(), "d1:ai1e1:bi2ee"));
  CPPUNIT_ASSERT(torrent::raw_bencode_equal_c_str(map_raw[key_raw_a].as_raw_bencode().as_raw_map(), "1:ai1e1:bi2e"));
}

void
ObjectStaticMapTest::test_read_raw_types() {
  test_raw_types_type map_raw;

  CPPUNIT_ASSERT(static_map_read_bencode(map_raw, "d" "1:ei1e" "1:lli1ei2ee" "1:md1:ai1e1:bi2ee" "1:s2:ab" "e"));
  CPPUNIT_ASSERT(torrent::raw_bencode_equal_c_str(map_raw[key_raw_types_empty].as_raw_bencode(), "i1e"));
  CPPUNIT_ASSERT(torrent::raw_bencode_equal_c_str(map_raw[key_raw_types_str].as_raw_string(), "ab"));
  CPPUNIT_ASSERT(torrent::raw_bencode_equal_c_str(map_raw[key_raw_types_list].as_raw_list(), "i1ei2e"));
  CPPUNIT_ASSERT(torrent::raw_bencode_equal_c_str(map_raw[key_raw_types_map].as_raw_map(), "1:ai1e1:bi2e"));
}

void
ObjectStaticMapTest::test_read_multiple() {
  test_multiple_type map_normal;

  CPPUNIT_ASSERT(static_map_read_bencode(map_normal, "d1:ai1ee"));
  CPPUNIT_ASSERT(map_normal[key_multiple_a].as_value() == 1);
  CPPUNIT_ASSERT(map_normal[key_multiple_b].is_empty());
  CPPUNIT_ASSERT(map_normal[key_multiple_c].is_empty());
  
  map_normal = test_multiple_type();
  CPPUNIT_ASSERT(static_map_read_bencode(map_normal, "d1:ci3ee"));
  CPPUNIT_ASSERT(map_normal[key_multiple_a].is_empty());
  CPPUNIT_ASSERT(map_normal[key_multiple_b].is_empty());
  CPPUNIT_ASSERT(map_normal[key_multiple_c].as_value() == 3);
  
  map_normal = test_multiple_type();
  CPPUNIT_ASSERT(static_map_read_bencode(map_normal, "d1:ai1e1:ci3ee"));
  CPPUNIT_ASSERT(map_normal[key_multiple_a].as_value() == 1);
  CPPUNIT_ASSERT(map_normal[key_multiple_b].is_empty());
  CPPUNIT_ASSERT(map_normal[key_multiple_c].as_value() == 3);
  
  map_normal = test_multiple_type();
  CPPUNIT_ASSERT(static_map_read_bencode(map_normal, "d1:ai1e1:bi2e1:ci3ee"));
  CPPUNIT_ASSERT(map_normal[key_multiple_a].as_value() == 1);
//   CPPUNIT_ASSERT(map_normal[key_multiple_b].as_raw_bencode().as_raw_value().size() == 1);
//   CPPUNIT_ASSERT(map_normal[key_multiple_b].as_raw_bencode().as_raw_value().data()[0] == '2');
  CPPUNIT_ASSERT(map_normal[key_multiple_c].as_value() == 3);
}

void
ObjectStaticMapTest::test_read_dict() {
  test_dict_type map_normal;

  CPPUNIT_ASSERT(static_map_read_bencode(map_normal, "d1:ai1ee"));
  CPPUNIT_ASSERT(map_normal[key_dict_a_b].is_empty());

  CPPUNIT_ASSERT(static_map_read_bencode(map_normal, "d1:adee"));
  CPPUNIT_ASSERT(map_normal[key_dict_a_b].is_empty());

  CPPUNIT_ASSERT(static_map_read_bencode(map_normal, "d1:ad1:bi1eee"));
  CPPUNIT_ASSERT(map_normal[key_dict_a_b].as_value() == 1);
}

void
ObjectStaticMapTest::test_write_empty() {
  test_empty_type map_normal;
  
  CPPUNIT_ASSERT(static_map_write_bencode(map_normal, "de"));
}

void
ObjectStaticMapTest::test_write_single() {
  test_single_type map_value;
  
  CPPUNIT_ASSERT(static_map_write_bencode(map_value, "de"));

  map_value[key_single_a] = (int64_t)1;
  CPPUNIT_ASSERT(static_map_write_bencode(map_value, "d1:bi1ee"));

  map_value[key_single_a] = "test";
  CPPUNIT_ASSERT(static_map_write_bencode(map_value, "d1:b4:teste"));

  map_value[key_single_a] = torrent::Object::create_list();
  map_value[key_single_a].as_list().push_back("test");
  CPPUNIT_ASSERT(static_map_write_bencode(map_value, "d1:bl4:testee"));

  map_value[key_single_a] = torrent::raw_bencode("i1e", 3);
  CPPUNIT_ASSERT(static_map_write_bencode(map_value, "d1:bi1ee"));

//   map_value[key_single_a] = torrent::raw_value("1", 1);
//   CPPUNIT_ASSERT(static_map_write_bencode(map_value, "d1:bi1ee"));

  map_value[key_single_a] = torrent::raw_string("test", 4);
  CPPUNIT_ASSERT(static_map_write_bencode(map_value, "d1:b4:teste"));

  map_value[key_single_a] = torrent::raw_list("1:a2:bb", strlen("1:a2:bb"));
  CPPUNIT_ASSERT(static_map_write_bencode(map_value, "d1:bl1:a2:bbee"));

  map_value[key_single_a] = torrent::raw_map("1:a2:bb", strlen("1:a2:bb"));
  CPPUNIT_ASSERT(static_map_write_bencode(map_value, "d1:bd1:a2:bbee"));
}

void
ObjectStaticMapTest::test_write_multiple() {
//   test_multiple_type map_value;
  
//   CPPUNIT_ASSERT(static_map_write_bencode(map_value, "de"));
}
