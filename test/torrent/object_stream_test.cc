#include "config.h"

#include <sstream>
#include <torrent/object.h>

#import "object_stream_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(ObjectStreamTest);

static const char* ordered_bencode = "d1:ei0e4:ipv44:XXXX4:ipv616:XXXXXXXXXXXXXXXX1:md11:upload_onlyi3e12:ut_holepunchi4e11:ut_metadatai2e6:ut_pexi1ee13:metadata_sizei15408e1:pi16033e4:reqqi255e1:v15:uuTorrent 1.8.46:yourip4:XXXXe";
static const char* unordered_bencode = "d1:ei0e1:md11:upload_onlyi3e12:ut_holepunchi4e11:ut_metadatai2e6:ut_pexi1ee4:ipv44:XXXX4:ipv616:XXXXXXXXXXXXXXXX13:metadata_sizei15408e1:pi16033e4:reqqi255e1:v15:uuTorrent 1.8.46:yourip4:XXXXe";

static const char* string_bencode = "32:aaaaaaaabbbbbbbbccccccccdddddddd";

static torrent::Object
create_bencode(const char* str) {
  torrent::Object obj;
  std::stringstream stream(str);

  stream >> obj;

  CPPUNIT_ASSERT(!stream.fail());
  return obj;
}

static bool
compare_bencode(const torrent::Object& obj, const char* str, uint32_t skip_mask = 0) {
  char buffer[256];
  std::memset(buffer, 0, 256);
  torrent::object_write_bencode(buffer, buffer + 256, &obj, skip_mask).second;

  return strcmp(buffer, str) == 0;
}

void
ObjectStreamTest::testInputOrdered() {
  torrent::Object orderedObj   = create_bencode(ordered_bencode);
  torrent::Object unorderedObj = create_bencode(unordered_bencode);

  CPPUNIT_ASSERT(!(orderedObj.flags() & torrent::Object::flag_unordered));
  CPPUNIT_ASSERT(unorderedObj.flags() & torrent::Object::flag_unordered);
}

void
ObjectStreamTest::testInputNullKey() {
  torrent::Object obj = create_bencode("d0:i1e5:filesi2ee");

  CPPUNIT_ASSERT(!(obj.flags() & torrent::Object::flag_unordered));
}

void
ObjectStreamTest::testOutputMask() {
  torrent::Object normalObj = create_bencode("d1:ai1e1:bi2e1:ci3ee");

  CPPUNIT_ASSERT(compare_bencode(normalObj, "d1:ai1e1:bi2e1:ci3ee"));

  normalObj.get_key("b").set_flags(torrent::Object::flag_session_data);
  normalObj.get_key("c").set_flags(torrent::Object::flag_static_data);

  CPPUNIT_ASSERT(compare_bencode(normalObj, "d1:ai1e1:ci3ee", torrent::Object::flag_session_data));
}

//
// Testing for bugs in bencode write.
//

// Dummy function that invalidates the buffer once called.

torrent::object_buffer_t
object_write_to_invalidate(void* data, torrent::object_buffer_t buffer) {
  return torrent::object_buffer_t(buffer.second, buffer.second);
}

void
ObjectStreamTest::testBuffer() {
  char raw_buffer[16];
  torrent::object_buffer_t buffer(raw_buffer, raw_buffer + 16);

  torrent::Object obj = create_bencode(string_bencode);

  object_write_bencode_c(&object_write_to_invalidate, NULL, buffer, &obj);
}
