#include "config.h"

#include "string_utf8.h"

#include "torrent/object.h"
#include "torrent/utils/string_manip.h"

namespace torrent {

namespace {

// Checks if a string is valid UTF-8 by analyzing byte patterns.
bool is_valid_utf8(const std::string& str) {
  auto* bytes = reinterpret_cast<const unsigned char*>(str.c_str());

  while (*bytes != 0x00) {
    int num = 0;

    if ((*bytes & 0x80) == 0x00)
      num = 1;
    else if ((*bytes & 0xE0) == 0xC0)
      num = 2;
    else if ((*bytes & 0xF0) == 0xE0)
      num = 3;
    else if ((*bytes & 0xF8) == 0xF0)
      num = 4;
    else
      return false; // Invalid leading byte

    bytes++;

    for (int i = 1; i < num; ++i) { // Check continuation bytes
      if ((*bytes & 0xC0) != 0x80)
        return false;

      bytes++;
    }
  }

  return true;
}

} // namespace

void
string_utf8::reset(const std::string& str) {
  m_base64.clear();

  m_str     = str;
  m_is_utf8 = is_valid_utf8(str);
}

Object
string_utf8::object_base64() const {
  auto obj = Object(base64());
  obj.set_flags(Object::flag_base64);

  return obj;
}

Object
string_utf8::object_utf8_or_base64() const {
  if (!m_is_utf8) {
    auto obj = Object(base64());
    obj.set_flags(Object::flag_base64);

    return obj;
  }

  return Object(m_str);
}

void
string_utf8::reset_base64() const {
  m_base64 = utils::transform_to_hex_str(m_str);
}

} // namespace torrent
