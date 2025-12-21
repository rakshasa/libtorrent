#include "config.h"

#include <rak/string_manip.h>

#include "hash_string.h"

namespace torrent {

// TODO: Move to src/utils.
static bool
hash_string_is_alnum(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

static bool
hash_string_is_hex(char c) {
  return (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9');
}

const char*
hash_string_from_hex_c_str(const char* first, HashString& hash) {
  const char* hash_first = first;

  torrent::HashString::iterator itr = hash.begin();

  while (itr != hash.end()) {
    if (!hash_string_is_hex(*first) || !hash_string_is_hex(*(first + 1)))
      return hash_first;

    *itr++ = (rak::hexchar_to_value(*first) << 4) + rak::hexchar_to_value(*(first + 1));
    first += 2;
  }

  return first;
}

char*
hash_string_to_hex(const HashString& hash, char* first) {
  return rak::transform_hex(hash.begin(), hash.end(), first);
}

std::string
hash_string_to_hex_str(const HashString& hash) {
  std::string str(HashString::size_data * 2, '\0');
  rak::transform_hex(hash.begin(), hash.end(), str.begin());

  return str;
}

// TODO: Replace rak::copy_escape_html.
std::string
hash_string_to_html_str(const HashString& hash) {
  std::string str;
  str.reserve(HashString::size_data * 3);

  for (char c : hash) {
    if (hash_string_is_alnum(c) || c == '-') {
      str.push_back(c);
      continue;
    }

    auto convert = [](unsigned char c) {
      if (c < 10)
        return '0' + c;

      return 'A' + c - 10;
    };

    str.push_back('%');
    str.push_back(convert(c >> 4));
    str.push_back(convert(c & 0x0F));
  }

  return str;
}

} // namespace torrent
