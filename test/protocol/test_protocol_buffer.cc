#include "config.h"

#include "test_protocol_buffer.h"

#include <cstring>

#include "net/protocol_buffer.h"
#include "torrent/exceptions.h"

CPPUNIT_TEST_SUITE_REGISTRATION(test_protocol_buffer);

namespace {

using buffer_type = torrent::ProtocolBuffer<64>;

void
fill_buffer(buffer_type& buffer, buffer_type::size_type filled) {
  buffer.reset();
  buffer.set_end(filled);
  buffer.reset_position();
}

} // namespace

void
test_protocol_buffer::test_consume_within_range() {
  buffer_type buffer;
  fill_buffer(buffer, 10);

  CPPUNIT_ASSERT(!buffer.consume(4));
  CPPUNIT_ASSERT_EQUAL(buffer_type::size_type{6}, buffer.remaining());

  CPPUNIT_ASSERT(buffer.consume(6));
  CPPUNIT_ASSERT_EQUAL(buffer_type::size_type{0}, buffer.remaining());
}

// remaining() is a uint16_t subtraction, so a position past the end reports a length bounded by the
// type rather than by the buffer, and move_unused() hands that length straight to memmove().
void
test_protocol_buffer::test_consume_past_end_is_rejected() {
  buffer_type buffer;
  fill_buffer(buffer, 10);

  CPPUNIT_ASSERT_THROW(buffer.consume(16), torrent::internal_error);
  CPPUNIT_ASSERT(buffer.remaining() <= buffer.reserved());
}

void
test_protocol_buffer::test_consume_negative_is_rejected() {
  buffer_type buffer;
  fill_buffer(buffer, 10);

  CPPUNIT_ASSERT(!buffer.consume(4));
  CPPUNIT_ASSERT_THROW(buffer.consume(-8), torrent::internal_error);
  CPPUNIT_ASSERT(buffer.remaining() <= buffer.reserved());
}
