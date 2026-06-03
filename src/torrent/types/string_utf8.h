#ifndef LIBTORRENT_TORRENT_TYPES_STRING_UTF8_H
#define LIBTORRENT_TORRENT_TYPES_STRING_UTF8_H

#include <string>
#include <torrent/common.h>

namespace torrent {

class LIBTORRENT_EXPORT string_utf8 {
public:
  string_utf8() = default;
  ~string_utf8() = default;

  static string_utf8 from_string(const std::string& str);

  bool               empty() const;

  bool               is_utf8() const;

  const std::string& str() const;
  const char*        c_str() const;

  // Generates a cached encoded strings, not thread-safe.
  const std::string& hex() const;
  const std::string& base64() const;

  Object             object_hex() const;
  Object             object_base64() const;
  Object             object_binary() const;
  Object             object_utf8_or_hex() const;
  Object             object_utf8_or_base64() const;
  Object             object_utf8_or_binary() const;

  void               reset(const std::string& str);

private:
  void               reset_hex() const;
  void               reset_base64() const;

  bool               m_is_utf8{true};

  std::string         m_str;

  mutable std::string m_hex;
  mutable std::string m_base64;
};

inline bool               string_utf8::empty() const   { return m_str.empty(); }
inline bool               string_utf8::is_utf8() const { return m_is_utf8; }
inline const std::string& string_utf8::str() const     { return m_str; }
inline const char*        string_utf8::c_str() const   { return m_str.c_str(); }

inline string_utf8
string_utf8::from_string(const std::string& str) {
  string_utf8 result;
  result.reset(str);

  return result;
}

inline const std::string&
string_utf8::hex() const  {
  if (!m_str.empty() && m_hex.empty())
    reset_hex();

  return m_hex;
}

inline const std::string&
string_utf8::base64() const  {
  if (!m_str.empty() && m_base64.empty())
    reset_base64();

  return m_base64;
}

} // namespace torrent

#endif
