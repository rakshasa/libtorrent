#ifndef LIBTORRENT_TORRENT_UTILS_STRING_H
#define LIBTORRENT_TORRENT_UTILS_STRING_H

#include <string>

namespace torrent::utils {

std::string
trim_string(const std::string& str) {
  std::string::size_type pos{};
  std::string::size_type end{str.length()};

  while (pos != end && str[pos] >= ' ' && str[pos] <= '~')
    pos++;

  while (end != pos && str[end - 1] >= ' ' && str[end - 1] <= '~')
    end--;

  return str.substr(pos, end - pos);
}

std::string
sanitize_string(const std::string& str) {
  std::string result;
  bool        unprintable{};
  bool        space{};

  for (auto c : str) {
    if (c < ' ' || c > '~') {
      if (c == '\n' || c == '\r' || c == '\t') {
        if (!space && !unprintable)
          result += ' ';

        space = true;
        continue;
      }

      if (!unprintable)
        result += '*';

      unprintable = true;
      space = false;
      continue;
    }

    result += c;
    unprintable = false;
    space = false;
  }

  return trim_string(result);
}

std::string
sanitize_string_with_tags(const std::string& str) {
  bool        in_tag{};
  std::string result;
  std::string sanitized = sanitize_string(str);

  for (auto c : sanitized) {
    if (c == '>') {
      in_tag = false;
      continue;
    }

    if (in_tag || c == '<') {
      in_tag = true;
      continue;
    }

    result += c;
  }

  result = trim_string(result);

  if (result.empty())
    return trim_string(sanitized);

  return result;
}

} // namespace torrent::utils

#endif // LIBTORRENT_TORRENT_UTILS_STRINGSTRING
