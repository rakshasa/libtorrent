#include "config.h"

#include "test_string_utf8.h"

#include <cstdint>
#include <initializer_list>
#include <string>

#include "torrent/object.h"
#include "torrent/types/string_utf8.h"
#include "torrent/utils/string_manip.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_string_utf8, "torrent");

namespace {

constexpr uint32_t binary_flags = torrent::Object::flag_hex | torrent::Object::flag_base64 | torrent::Object::flag_as_binary;

// The objects hold the string itself when it passes their check, and are marked as binary otherwise.
void
check_objects(const std::string& message, const std::string& str, bool passes,
              const torrent::Object& as_binary, const torrent::Object& base64, const torrent::Object& hex) {
  if (passes) {
    CPPUNIT_ASSERT_EQUAL_MESSAGE(message, uint32_t{}, as_binary.flags() & binary_flags);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(message, uint32_t{}, base64.flags() & binary_flags);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(message, uint32_t{}, hex.flags() & binary_flags);
    CPPUNIT_ASSERT_MESSAGE(message, as_binary.as_string() == str);
    CPPUNIT_ASSERT_MESSAGE(message, base64.as_string() == str);
    CPPUNIT_ASSERT_MESSAGE(message, hex.as_string() == str);
    return;
  }

  CPPUNIT_ASSERT_EQUAL_MESSAGE(message, torrent::Object::flag_as_binary, as_binary.flags() & binary_flags);
  CPPUNIT_ASSERT_EQUAL_MESSAGE(message, torrent::Object::flag_base64 | torrent::Object::flag_as_binary, base64.flags() & binary_flags);
  CPPUNIT_ASSERT_EQUAL_MESSAGE(message, torrent::Object::flag_hex | torrent::Object::flag_as_binary, hex.flags() & binary_flags);
  CPPUNIT_ASSERT_MESSAGE(message, as_binary.as_string() == str);
  CPPUNIT_ASSERT_MESSAGE(message, base64.as_string() == torrent::utils::transform_to_base64(str));
  CPPUNIT_ASSERT_MESSAGE(message, hex.as_string() == torrent::utils::transform_to_hex_str(str));
}

void
check_string(const std::string& str, bool utf8, bool text) {
  auto message = "hex: " + torrent::utils::transform_to_hex_str(str);
  auto value   = torrent::string_utf8::from_string(str);

  CPPUNIT_ASSERT_EQUAL_MESSAGE(message, utf8, torrent::utils::is_valid_utf8(str));
  CPPUNIT_ASSERT_EQUAL_MESSAGE(message, utf8, value.is_utf8());
  CPPUNIT_ASSERT_EQUAL_MESSAGE(message, text, value.is_utf8_text());

  check_objects(message + ", utf8_or", str, utf8, value.object_utf8_or_as_binary(), value.object_utf8_or_base64(), value.object_utf8_or_hex());
  check_objects(message + ", utf8_text_or", str, text, value.object_utf8_text_or_as_binary(), value.object_utf8_text_or_base64(), value.object_utf8_text_or_hex());
}

// Each string is also checked between two ASCII letters.
void
check_strings(std::initializer_list<std::string> strings, bool utf8, bool text) {
  for (const auto& str : strings) {
    check_string(str, utf8, text);
    check_string("a" + str + "b", utf8, text);
  }
}

void
check_valid(std::initializer_list<std::string> strings, bool utf8, bool text) {
  for (const auto& str : strings) {
    auto message = "hex: " + torrent::utils::transform_to_hex_str(str);

    CPPUNIT_ASSERT_EQUAL_MESSAGE(message, utf8, torrent::utils::is_valid_utf8(str));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(message, text, torrent::utils::is_valid_utf8_text(str));
  }
}

} // namespace

void
test_string_utf8::test_ascii() {
  std::string printable;

  for (char c = 0x20; c < 0x7f; c++)
    printable += c;

  check_strings({"", "name.torrent", "<&>\"'", printable}, true, true);
}

void
test_string_utf8::test_multibyte() {
  check_strings({"\xc3\xa9",                 // U+00E9
                 "\xd1\x87\xd0\xb0\xd0\xbe", // Cyrillic
                 "\xe2\x82\xac",             // U+20AC
                 "\xef\xbf\xbd",             // U+FFFD
                 "\xf0\x9f\x98\x8a"},        // U+1F60A
                true, true);
}

void
test_string_utf8::test_tab_and_line_feed() {
  check_strings({"\t", "\n", "line\tone\nline\ttwo\n"}, true, true);
}

void
test_string_utf8::test_carriage_return() {
  check_strings({"\r", "line\r\n"}, true, false);
}

void
test_string_utf8::test_c0_controls() {
  check_strings({std::string(1, '\0'), std::string("nul\0name", 8), "\x1b[31m", "\xc3\xa9\x01"}, true, false);

  for (char c = 0x01; c < 0x20; c++) {
    if (c == '\t' || c == '\n' || c == '\r')
      continue;

    check_strings({std::string(1, c)}, true, false);
  }
}

// Only the C0 controls are refused, DEL and the C1 controls stay text.
void
test_string_utf8::test_c1_controls() {
  check_strings({"\x7f",      // DEL
                 "\xc2\x80",  // U+0080
                 "\xc2\x9f"}, // U+009F
                true, true);
}

void
test_string_utf8::test_invalid_utf8() {
  check_strings({"\xc3",
                 "\xe2\x82",
                 "\xf0\x9f\x98",
                 "\x80",
                 "\xc3(",
                 "\xc3\xa9\xc3",
                 "\xf8\x88\x80\x80\x80",
                 "\xff",
                 "\x01\xc3"},
                false, false);
}

void
test_string_utf8::test_is_valid_utf8_text() {
  check_valid({"", "\t", "\n", "line\tone\nline\ttwo\n", "\xc3\xa9", "\xf0\x9f\x98\x8a"}, true, true);
  check_valid({std::string(1, '\0'), "\r", "\x1f", "a\rb"}, true, false);
  check_valid({"\xc3", "\xff", "\xc3\xa9\xc3"}, false, false);
}
