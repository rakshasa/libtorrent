#include "config.h"

#include "string_utf8.h"

#include <openssl/evp.h>

#include "torrent/object.h"
#include "torrent/utils/string_manip.h"

namespace torrent {

namespace {

// Checks if a string is valid UTF-8 by analyzing byte patterns.
bool
is_valid_utf8(const std::string& str) {
  auto itr       = str.begin();
  const auto end = str.end();

  while (itr != end) {
    int  num{};
    auto byte = static_cast<unsigned char>(*itr);

    // Verify leading byte.
    if ((byte & 0x80) == 0x00)
      num = 1;
    else if ((byte & 0xE0) == 0xC0)
      num = 2;
    else if ((byte & 0xF0) == 0xE0)
      num = 3;
    else if ((byte & 0xF8) == 0xF0)
      num = 4;
    else
      return false;

    ++itr;

    // Check continuation bytes.
    for (int i = 1; i < num; ++i) {
      if (itr == end)
        return false;

      if ((static_cast<unsigned char>(*itr) & 0xC0) != 0x80)
        return false;

      ++itr;
    }
  }

  return true;
}

std::string
openssl_base64_encode(const std::string& src) {
  if (src.empty()) return {};

  std::string result((4 * ((src.size() + 2) / 3)), '\0');

  int actual_length = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(result.data()),
                                      reinterpret_cast<const unsigned char*>(src.data()),
                                      src.size());

  result.resize(actual_length);
  return result;
}

} // namespace

void
string_utf8::reset(const std::string& str) {
  m_hex.clear();
  m_base64.clear();

  m_str     = str;
  m_is_utf8 = is_valid_utf8(str);
}

Object
string_utf8::object_hex() const {
  auto obj = Object(hex());
  obj.set_flags(Object::flag_hex);

  return obj;
}

Object
string_utf8::object_base64() const {
  auto obj = Object(base64());
  obj.set_flags(Object::flag_base64);

  return obj;
}

Object
string_utf8::object_binary() const {
  auto obj = Object(m_str);
  obj.set_flags(Object::flag_binary);

  return obj;
}

Object
string_utf8::object_utf8_or_hex() const {
  if (!m_is_utf8) {
    auto obj = Object(hex());
    obj.set_flags(Object::flag_hex);

    return obj;
  }

  return Object(m_str);
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

Object
string_utf8::object_utf8_or_binary() const {
  if (!m_is_utf8) {
    auto obj = Object(m_str);
    obj.set_flags(Object::flag_binary);

    return obj;
  }

  return Object(m_str);
}

void
string_utf8::reset_hex() const {
  m_hex = utils::transform_to_hex_str(m_str);
}

void
string_utf8::reset_base64() const {
  m_base64 = openssl_base64_encode(m_str);
}

} // namespace torrent
