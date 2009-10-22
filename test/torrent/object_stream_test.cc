#include "config.h"

#include <sstream>
#include <torrent/object.h>

#import "object_stream_test.h"

CPPUNIT_TEST_SUITE_REGISTRATION(ObjectStreamTest);

static const char* ordered_bencode = "d1:ei0e4:ipv44:XXXX4:ipv616:XXXXXXXXXXXXXXXX1:md11:upload_onlyi3e12:ut_holepunchi4e11:ut_metadatai2e6:ut_pexi1ee13:metadata_sizei15408e1:pi16033e4:reqqi255e1:v15:uuTorrent 1.8.46:yourip4:XXXXe";
static const char* unordered_bencode = "d1:ei0e1:md11:upload_onlyi3e12:ut_holepunchi4e11:ut_metadatai2e6:ut_pexi1ee4:ipv44:XXXX4:ipv616:XXXXXXXXXXXXXXXX13:metadata_sizei15408e1:pi16033e4:reqqi255e1:v15:uuTorrent 1.8.46:yourip4:XXXXe";

static torrent::Object
create_bencode(const char* str) {
  torrent::Object obj;
  std::stringstream stream(str);

  stream >> obj;

  CPPUNIT_ASSERT(!stream.fail());
  return obj;
}

void
ObjectStreamTest::testInputOrdered() {
  torrent::Object orderedObj   = create_bencode(ordered_bencode);
  torrent::Object unorderedObj = create_bencode(unordered_bencode);

  CPPUNIT_ASSERT(!(orderedObj.flags() & torrent::Object::flag_unordered));
  CPPUNIT_ASSERT(unorderedObj.flags() & torrent::Object::flag_unordered);
}
