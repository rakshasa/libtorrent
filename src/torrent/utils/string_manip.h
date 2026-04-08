#ifndef LIBTORRENT_TORRENT_UTILS_STRING_MANIP_H
#define LIBTORRENT_TORRENT_UTILS_STRING_MANIP_H

#include <string>
#include <string_view>
#include <torrent/common.h>

namespace torrent::utils {

// TODO: Add a copy_escape_html() version that copies to a perfect sized std::string.

std::string_view trim_spaces(std::string_view s) LIBTORRENT_EXPORT;

std::string      string_with_escape_codes(const std::string& str) LIBTORRENT_EXPORT;

std::string      sanitize_string(const std::string& str) LIBTORRENT_EXPORT;
std::string      sanitize_string_with_escape_codes(const std::string& str) LIBTORRENT_EXPORT;
std::string      sanitize_string_with_tags(const std::string& str) LIBTORRENT_EXPORT;

char             hex_to_value(char c);
char             value_to_hex0(char value);
char             value_to_hex1(char value);

template <typename Sequence>
std::string copy_escape_html(const Sequence& src);

template <typename SrcItr>
std::string copy_escape_html(SrcItr src_first, SrcItr src_last);

template <typename SrcItr, typename DestItr>
DestItr     copy_escape_html(SrcItr src_first, SrcItr src_last, DestItr dst_first, DestItr dst_last);

template <typename Sequence>
std::string transform_hex(const Sequence& src);

template <typename SrcItr>
std::string transform_hex(SrcItr src_first, SrcItr src_last);

template <typename SrcItr, typename DestItr>
DestItr     transform_hex(SrcItr src_first, SrcItr src_last, DestItr dst_first, DestItr dst_last);


//
// Conversion between hex char and value.
//

// Could optimize this abit.
inline char
hexchar_to_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'A' && c <= 'F')
    return 10 + c - 'A';

  if (c >= 'a' && c <= 'f')
    return 10 + c - 'a';

  return 0;
}

inline char
value_to_hex0(char value) {
  value = value & 0x0f;

  if (value < 10)
    return '0' + value;

  return 'A' + (value - 10);
}

inline char
value_to_hex1(char value) {
  value = (value >> 4) & 0x0f;

  if (value < 10)
    return '0' + value;

  return 'A' + (value - 10);
}

//
// Escape all characters that are not alphanumeric or '-' with %XX.
//

template <typename SrcItr>
inline std::string
copy_escape_html(SrcItr src_first, SrcItr src_last) {
  std::string dest;
  dest.reserve(std::distance(src_first, src_last) * 3);

  while (src_first != src_last) {
    if ((*src_first >= 'A' && *src_first <= 'Z') ||
        (*src_first >= 'a' && *src_first <= 'z') ||
        (*src_first >= '0' && *src_first <= '9') || *src_first == '-') {

      dest += *src_first;

    } else {
      dest += '%';
      dest += value_to_hex1(*src_first);
      dest += value_to_hex0(*src_first);
    }

    ++src_first;
  }

  return dest;
}

template <typename SrcItr, typename DestItr>
inline DestItr
copy_escape_html(SrcItr src_first, SrcItr src_last, DestItr dst_first, DestItr dst_last) {
  while (src_first != src_last) {
    if ((*src_first >= 'A' && *src_first <= 'Z') ||
        (*src_first >= 'a' && *src_first <= 'z') ||
        (*src_first >= '0' && *src_first <= '9') || *src_first == '-') {

      if (dst_first == dst_last) break; else *(dst_first++) = *src_first;

    } else {
      if (dst_first == dst_last) break; else *(dst_first++) = '%';
      if (dst_first == dst_last) break; else *(dst_first++) = value_to_hex1(*src_first);
      if (dst_first == dst_last) break; else *(dst_first++) = value_to_hex0(*src_first);
    }

    ++src_first;
  }

  return dst_first;
}

template <typename Sequence>
inline std::string
copy_escape_html(const Sequence& src) {
  return copy_escape_html(src.begin(), src.end());
}

//
// Transform a sequence of bytes to a hex string.
//

template <typename SrcItr>
inline std::string
transform_hex(SrcItr src_first, SrcItr src_last) {
  std::string dest;
  dest.reserve(std::distance(src_first, src_last) * 2);

  while (src_first != src_last) {
    dest += value_to_hex1(*src_first);
    dest += value_to_hex0(*src_first);

    ++src_first;
  }

  return dest;
}

template <typename SrcItr, typename DestItr>
DestItr
transform_hex(SrcItr src_first, SrcItr src_last, DestItr dst_first, DestItr dst_last) {
  while (src_first != src_last) {
    if (dst_first == dst_last)
      break;

    *(dst_first++) = value_to_hex1(*src_first);

    if (dst_first == dst_last)
      break;

    *(dst_first++) = value_to_hex0(*src_first);

    ++src_first;
  }

  return dst_first;
}

template <typename Sequence>
inline std::string
transform_hex(const Sequence& src) {
  return transform_hex(src.begin(), src.end());
}

} // namespace torrent::utils

#endif // LIBTORRENT_TORRENT_UTILS_STRING_MANIP_H
