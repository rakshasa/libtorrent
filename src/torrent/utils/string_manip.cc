#include "config.h"

#include "torrent/utils/string_manip.h"

#include "torrent/exceptions.h"

namespace torrent::utils {

namespace {

inline bool
is_hex_char(char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'A' && c <= 'F') ||
         (c >= 'a' && c <= 'f');
}

inline char
from_hex_char(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'A' && c <= 'F')
    return 10 + c - 'A';

  return 10 + c - 'a';
}

inline char
to_hex_char(char val, bool pos) {
  if (pos)
    val = (val >> 4) & 0x0F;
  else
    val = val & 0x0F;

  if (val < 10)
    return '0' + val;

  return 'A' + (val - 10);
}

} // namespace

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
string_from_hex_or_empty(const char* hex_begin, const char* hex_end) {
  std::string result;
  result.reserve((hex_end - hex_begin) / 2);

  const char* itr = hex_begin;

  while (itr + 1 < hex_end) {
    if (!is_hex_char(*itr) || !is_hex_char(*(itr + 1)))
      return std::string();

    char val = (from_hex_char(*itr) << 4) + from_hex_char(*(itr + 1));
    result += val;

    itr += 2;
  }

  if (itr != hex_end)
    return std::string();

  return result;
}

bool
string_from_hex_with_check(const char* hex_begin, const char* hex_end, char* dest_begin, char* dest_end) {
  const char* itr      = hex_begin;
  char*       dest_itr = dest_begin;

  while (itr + 1 < hex_end && dest_itr < dest_end) {
    if (!is_hex_char(*itr) || !is_hex_char(*(itr + 1)))
      return false;

    char val = (from_hex_char(*itr) << 4) + from_hex_char(*(itr + 1));
    *(dest_itr++) = val;

    itr += 2;
  }

  return itr == hex_end && dest_itr == dest_end;
}

std::string
string_to_hex(const char* begin, const char* end) {
  std::string result;
  result.reserve((end - begin) * 2);

  for (const char* itr = begin; itr != end; ++itr) {
    result += to_hex_char(*itr, true);
    result += to_hex_char(*itr, false);
  }

  return result;
}

char*
string_to_hex(const char* begin, const char* end, char* dest) {
  for (const char* itr = begin; itr != end; ++itr) {
    *(dest++) = to_hex_char(*itr, true);
    *(dest++) = to_hex_char(*itr, false);
  }

  return dest;
}

std::string
string_with_escape_codes(const std::string& str) {
  std::string result;

  for (auto c : str) {
    if (c < ' ' || c > '~') {
      result += '%' + to_hex_char(c, true) + to_hex_char(c, false);
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
    unprintable = false;
    space = false;
  }

  return trim_string(result);
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

      result += '%' + to_hex_char(c, true) + to_hex_char(c, false);

      space = false;
      continue;
    }

    result += c;
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
