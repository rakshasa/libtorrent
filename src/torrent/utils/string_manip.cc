#include "config.h"

#include "torrent/utils/string_manip.h"

#include <exception>

#include "torrent/common.h"

namespace torrent::utils {

std::string_view
trim_spaces(std::string_view s) {
  auto first = std::find_if(s.begin(), s.end(), [](unsigned char ch) {
    return ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' && ch != '\f' && ch != '\v';
  });

  auto last = std::find_if(s.rbegin(), std::make_reverse_iterator(first), [](unsigned char ch) {
    return ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' && ch != '\f' && ch != '\v';
  }).base();

  if (first >= last)
    return std::string_view();

  return std::string_view(first, last - first);
}

std::string
trim_spaces_str(std::string_view s) {
  return std::string(trim_spaces(s));
}

std::string
string_with_escape_codes(const std::string& str) {
  std::string result;

  for (auto c : str) {
    if (c < ' ' || c > '~') {
      result += '%';
      result += value_to_hex1(c);
      result += value_to_hex0(c);
      continue;
    }

    result += c;
  }

  return result;
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

    unprintable  = false;
    space        = false;
  }

  return trim_spaces_str(result);
}

std::string
sanitize_string_with_escape_codes(const std::string& str) {
  std::string result;
  bool        space{};

  for (auto c : str) {
    if (c < ' ' || c > '~') {
      if (c == '\n' || c == '\r' || c == '\t') {
        if (!space)
          result += ' ';

        space = true;
        continue;
      }

      result += '%';
      result += value_to_hex1(c);
      result += value_to_hex0(c);

      space = false;
      continue;
    }

    result += c;
    space = false;
  }

  return trim_spaces_str(result);
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

  result = trim_spaces_str(result);

  if (result.empty())
    return trim_spaces_str(sanitized);

  return result;
}

} // namespace torrent::utils
