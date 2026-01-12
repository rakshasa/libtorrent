#include "config.h"

#include "torrent/hash_string.h"

#include "torrent/utils/string_manip.h"

namespace {

inline bool
hash_string_is_alnum(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

} // namespace

namespace torrent {

const char*
hash_string_from_hex_c_str(const char* first, HashString& hash) {
  if (!utils::string_from_hex_with_check(first, first + HashString::size_data * 2, hash.begin(), hash.end()))
    return first;

  return first + HashString::size_data * 2;
}

char*
hash_string_to_hex(const HashString& hash, char* first) {
  return utils::string_to_hex(hash.begin(), hash.end(), first);
}

std::string
hash_string_to_hex_str(const HashString& hash) {
  return utils::string_to_hex(hash.begin(), hash.end());
}

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
