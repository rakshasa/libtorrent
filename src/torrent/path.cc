#include "config.h"

#include <algorithm>

#include "path.h"

namespace torrent {

void
Path::insert_path(iterator pos, const std::string& path) {
  std::string::const_iterator first = path.begin();
  std::string::const_iterator last;

  while (first != path.end()) {
    last = std::find(first, path.end(), '/');
    pos  = insert(pos, string_utf8::from_string(std::string(first, last)));

    if (last == path.end())
      return;

    first = last;
    first++;
  }
}

std::string
Path::as_string() const {
  if (empty())
    return std::string();

  std::string s;

  for (const auto& c : *this) {
    s += '/';
    s += c.str();
  }

  return s;
}

} // namespace torrent
