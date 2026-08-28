#include "config.h"

#include "path.h"

#include <algorithm>

#include "torrent/exceptions.h"

namespace torrent {

bool
Path::is_prefix(const Path* prefix, const Path* path) {
  return prefix->size() <= path->size() &&
    std::equal(prefix->begin(), prefix->end(), path->begin(),
               [](const auto& l, const auto& r) { return l.str() == r.str(); });
}

bool
Path::is_valid_component(const std::string& name) {
  if (name.empty())
    return false;

  if (name == "." || name == "..")
    return false;

  if (std::any_of(name.begin(), name.end(), [](char c) { return c == '/' || c == '\0'; }))
    return false;

  return true;
}

void
Path::insert_path(iterator pos, const std::string& path) {
  if (std::find(path.begin(), path.end(), '\0') != path.end())
    throw input_error("Invalid path, contains null character.");

  auto first = path.begin();

  while (first != path.end()) {
    auto last = std::find(first, path.end(), '/');

    pos = insert(pos, string_utf8::from_string(std::string(first, last)));

    if (last == path.end())
      return;

    first = last + 1;
  }
}

void
Path::insert_component(iterator pos, const std::string& name) {
  if (!is_valid_component(name))
    throw input_error("Invalid path component name.");

  insert(pos, string_utf8::from_string(name));
}

bool
Path::compare_less(const Path* left, const Path* right) {
  return std::lexicographical_compare(left->begin(), left->end(), right->begin(), right->end(),
                                      [](const auto& l, const auto& r) { return l.str() < r.str(); });
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
