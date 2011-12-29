#include "config.h"

#include <sstream>
#include <cppunit/extensions/HelperMacros.h>
#include "torrent/object_stream.h"

#include "object_test_utils.h"

torrent::Object
create_bencode(const char* str) {
  torrent::Object obj;
  std::stringstream stream(str);

  stream >> obj;

  CPPUNIT_ASSERT(!stream.fail());
  return obj;
}

torrent::Object
create_bencode_c(const char* str) {
  torrent::Object obj;
  const char* last = str + strlen(str);

  CPPUNIT_ASSERT(object_read_bencode_c(str, last, &obj) == last);
  return obj;
}

// torrent::Object
// create_bencode_raw_bencode_c(const char* str) {
//   torrent::Object obj;
//   const char* last = str + strlen(str);

//   CPPUNIT_ASSERT(object_read_bencode_skip_c(str, last) == last);
//   return torrent::raw_bencode(str, std::distance(str, last));
// }

// torrent::Object
// create_bencode_raw_list_c(const char* str) {
//   torrent::Object obj;
//   const char* last = str + strlen(str);

//   CPPUNIT_ASSERT(object_read_bencode_skip_c(str, last) == last);
//   return torrent::raw_bencode(str, std::distance(str, last));
// }

bool
validate_bencode(const char* first, const char* last) {
  torrent::Object obj;
  return object_read_bencode_c(first, last, &obj) == last;
}

bool
compare_bencode(const torrent::Object& obj, const char* str, uint32_t skip_mask) {
  char buffer[256];
  std::memset(buffer, 0, 256);
  torrent::object_write_bencode(buffer, buffer + 256, &obj, skip_mask);

  return strcmp(buffer, str) == 0;
}
