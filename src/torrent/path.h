#ifndef LIBTORRENT_PATH_H
#define LIBTORRENT_PATH_H

#include <string>
#include <vector>
#include <torrent/common.h>
#include <torrent/types/string_utf8.h>

namespace torrent {

// Use a blank first path to get root and "." to get current dir.

class LIBTORRENT_EXPORT Path : private std::vector<string_utf8> {
public:
  using base_type = std::vector<string_utf8>;

  using const_iterator = base_type::const_iterator;

  using base_type::empty;
  using base_type::size;

  using base_type::front;
  using base_type::back;
  using base_type::begin;
  using base_type::end;

  using base_type::at;
  using base_type::operator[];

  void               insert_path(iterator pos, const std::string& path);
  void               push_back(const std::string& path);

  // Return the path as a string with '/' deliminator. The deliminator
  // is only inserted between path elements.
  std::string        as_string() const;

  std::string        encoding() const                     { return m_encoding; }
  void               set_encoding(const std::string& enc) { m_encoding = enc; }

  base_type*         base()                               { return this; }
  const base_type*   base() const                         { return this; }

private:
  std::string        m_encoding;
};

inline void Path::push_back(const std::string& path) { insert_path(end(), path); }

} // namespace torrent

#endif

