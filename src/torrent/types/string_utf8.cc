#include "config.h"

#include "string_utf8.h"

#include "torrent/object.h"
#include "torrent/utils/string_manip.h"

namespace torrent {

void
string_utf8::reset(const std::string& str) {
  m_hex.clear();
  m_base64.clear();

  m_str     = str;
  m_is_utf8 = utils::is_valid_utf8(str);
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
string_utf8::object_as_binary() const {
  auto obj = Object(m_str);
  obj.set_flags(Object::flag_as_binary);

  return obj;
}

Object
string_utf8::object_hex_as_binary() const {
  auto obj = Object(hex());
  obj.set_flags(Object::flag_hex | Object::flag_as_binary);

  return obj;
}

Object
string_utf8::object_base64_as_binary() const {
  auto obj = Object(base64());
  obj.set_flags(Object::flag_base64 | Object::flag_as_binary);

  return obj;
}

Object
string_utf8::object_utf8_or_hex() const {
  if (!m_is_utf8) {
    auto obj = Object(hex());
    obj.set_flags(Object::flag_hex | Object::flag_as_binary);

    return obj;
  }

  return Object(m_str);
}

Object
string_utf8::object_utf8_or_base64() const {
  if (!m_is_utf8) {
    auto obj = Object(base64());
    obj.set_flags(Object::flag_base64 | Object::flag_as_binary);

    return obj;
  }

  return Object(m_str);
}

Object
string_utf8::object_utf8_or_as_binary() const {
  if (!m_is_utf8) {
    auto obj = Object(m_str);
    obj.set_flags(Object::flag_as_binary);

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
  m_base64 = utils::transform_to_base64(m_str);
}

} // namespace torrent
