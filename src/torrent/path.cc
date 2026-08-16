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

bool
Path::compare_less(const Path* left, const Path* right) {
  return std::lexicographical_compare(left->begin(), left->end(), right->begin(), right->end(),
                                      [](const auto& l, const auto& r) { return l.str() < r.str(); });
}

bool
Path::is_prefix(const Path* prefix, const Path* path) {
  return prefix->size() <= path->size() &&
    std::equal(prefix->begin(), prefix->end(), path->begin(),
               [](const auto& l, const auto& r) { return l.str() == r.str(); });
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
