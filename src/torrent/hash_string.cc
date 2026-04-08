#include "config.h"

#include "hash_string.h"

#include "torrent/utils/string_manip.h"

namespace torrent {

const char*
hash_string_from_hex_c_str(const char* first, HashString& hash) {
  const char* hash_first = first;

  torrent::HashString::iterator itr = hash.begin();

  while (itr != hash.end()) {
    char high = *(first) << 4;
    char low  = *(first + 1);

    if (high >= '0' && high <= '9')
      high = high - '0';
    else if (high >= 'A' && high <= 'F')
      high = high - 'A' + 10;
    else if (high >= 'a' && high <= 'f')
      high = high - 'a' + 10;
    else
      return hash_first;

    if (low >= '0' && low <= '9')
      low = low - '0';
    else if (low >= 'A' && low <= 'F')
      low = low - 'A' + 10;
    else if (low >= 'a' && low <= 'f')
      low = low - 'a' + 10;
    else
      return hash_first;

    *itr++ = (high << 4) + low;

    first += 2;
  }

  return first;
}

char*
hash_string_to_hex(const HashString& hash, char* first, char* last) {
  return utils::transform_hex(hash.begin(), hash.end(), first, last);
}

std::string
hash_string_to_hex_str(const HashString& hash) {
  return utils::transform_hex(hash.begin(), hash.end());
}

std::string
hash_string_to_html_str(const HashString& hash) {
  return utils::copy_escape_html(hash.begin(), hash.end());
}

} // namespace torrent
