#ifndef LIBTORRENT_TORRENT_TYPES_STRING_UTF8_H
#define LIBTORRENT_TORRENT_TYPES_STRING_UTF8_H

#include <string>
#include <torrent/common.h>

namespace torrent {

class LIBTORRENT_EXPORT string_utf8 {
public:
  string_utf8() = default;
  ~string_utf8() = default;

  bool               is_utf8() const;

  const std::string& str() const;
  const char*        c_str() const;

  // Generates a cached base64-encoded string, not thread-safe.
  const std::string& base64() const;

  Object             object_utf8_or_base64() const;

  void               reset(const std::string& str);

private:
  void               reset_base64() const;

  bool               m_is_utf8{true};

  std::string         m_str;
  mutable std::string m_base64;
};

inline bool               string_utf8::is_utf8() const { return m_is_utf8; }
inline const std::string& string_utf8::str() const     { return m_str; }
inline const char*        string_utf8::c_str() const   { return m_str.c_str(); }

inline const std::string& string_utf8::base64() const  {
  if (!m_str.empty() && m_base64.empty())
    reset_base64();

  return m_base64;
}

} // namespace torrent

#endif
