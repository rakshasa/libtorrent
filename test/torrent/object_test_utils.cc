#include <sstream>
#include <cppunit/extensions/HelperMacros.h>
#include "torrent/object_stream.h"

#import "object_test_utils.h"

torrent::Object
create_bencode(const char* str) {
  torrent::Object obj;
  std::stringstream stream(str);

  stream >> obj;

  CPPUNIT_ASSERT(!stream.fail());
  return obj;
}

bool
compare_bencode(const torrent::Object& obj, const char* str, uint32_t skip_mask) {
  char buffer[256];
  std::memset(buffer, 0, 256);
  torrent::object_write_bencode(buffer, buffer + 256, &obj, skip_mask).second;

  return strcmp(buffer, str) == 0;
}
