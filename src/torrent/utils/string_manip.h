#ifndef LIBTORRENT_TORRENT_UTILS_STRING_MANIP_H
#define LIBTORRENT_TORRENT_UTILS_STRING_MANIP_H

#include <string>
#include <string_view>
#include <exception>
#include <torrent/common.h>
#include <torrent/exceptions.h>

namespace torrent::utils {

// TODO: Add a copy_escape_html() version that copies to a perfect sized std::string.
// TODO: Add copy/transform that take Sequence+itr-pair.

std::string_view trim_spaces(std::string_view s) LIBTORRENT_EXPORT;
std::string      trim_spaces_str(std::string_view s) LIBTORRENT_EXPORT;

std::string      string_with_escape_codes(const std::string& str) LIBTORRENT_EXPORT;

std::string      sanitize_string(const std::string& str) LIBTORRENT_EXPORT;
std::string      sanitize_string_with_escape_codes(const std::string& str) LIBTORRENT_EXPORT;
std::string      sanitize_string_with_tags(const std::string& str) LIBTORRENT_EXPORT;

char             hex_to_value_or_zero(char c);
char             hex_to_value_or_error(char c);
char             value_to_hex0(char value);
char             value_to_hex1(char value);

template <typename Container>
std::string      copy_escape_html(const Container& src);

template <typename SrcItr>
std::string      copy_escape_html(SrcItr src_first, SrcItr src_last);

// template <typename SrcItr, typename DestItr>
// DestItr          copy_escape_html(SrcItr src_first, SrcItr src_last, DestItr dst_first, DestItr dst_last);

// template <typename SrcContainer, typename DestItr>
// DestItr          copy_escape_html(const SrcContainer& src, DestItr dst_first, DestItr dst_last);

// template <typename SrcContainer, typename DstContainer>
// typename DstContainer::iterator copy_escape_html(SrcContainer& src, DstContainer dst);

// template <typename SrcItr, typename DstContainer>
// typename DstContainer::iterator copy_escape_html(SrcItr src_first, SrcItr src_last, DstContainer dst);

template <typename SrcItr, typename DestItr>
DestItr          transform_from_hex(SrcItr src_first, SrcItr src_last, DestItr dst_first, DestItr dst_last);

template <typename SrcContainer, typename DestItr>
DestItr          transform_from_hex(const SrcContainer& src, DestItr dst_first, DestItr dst_last);

template <typename SrcContainer, typename DstContainer>
typename DstContainer::iterator transform_from_hex(const SrcContainer& src, DstContainer dst);

template <typename SrcItr, typename DstContainer>
typename DstContainer::iterator transform_from_hex(SrcItr src_first, SrcItr src_last, DstContainer dst);

template <typename SrcItr, typename DestItr>
DestItr          transform_to_hex(SrcItr src_first, SrcItr src_last, DestItr dst_first, DestItr dst_last);

template <typename SrcContainer, typename DestItr>
DestItr          transform_to_hex(const SrcContainer& src, DestItr dst_first, DestItr dst_last);

template <typename SrcContainer, typename DstContainer>
typename DstContainer::iterator transform_to_hex(const SrcContainer& src, DstContainer dst);

template <typename SrcItr, typename DstContainer>
typename DstContainer::iterator transform_to_hex(SrcItr src_first, SrcItr src_last, DstContainer dst);

template <typename Container>
std::string      transform_to_hex_str(const Container& src);

template <typename SrcItr>
std::string      transform_to_hex_str(SrcItr src_first, SrcItr src_last);


//
// Conversion between hex char and value.
//

// Could optimize this abit.
inline char
hex_to_value_or_zero(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'A' && c <= 'F')
    return 10 + c - 'A';

  if (c >= 'a' && c <= 'f')
    return 10 + c - 'a';

  return 0;
}

inline char
hex_to_value_or_error(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';

  if (c >= 'A' && c <= 'F')
    return 10 + c - 'A';

  if (c >= 'a' && c <= 'f')
    return 10 + c - 'a';

  return -1;
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

template <typename Container>
std::string
copy_escape_html(const Container& src) {
  return copy_escape_html(src.begin(), src.end());
}

template <typename SrcItr>
std::string
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

// template <typename SrcItr, typename DestItr>
// DestItr
// copy_escape_html(SrcItr src_first, SrcItr src_last, DestItr dst_first, DestItr dst_last) {
//   while (src_first != src_last) {
//     if ((*src_first >= 'A' && *src_first <= 'Z') ||
//         (*src_first >= 'a' && *src_first <= 'z') ||
//         (*src_first >= '0' && *src_first <= '9') || *src_first == '-') {

//       if (dst_first == dst_last) break; else *(dst_first++) = *src_first;

//     } else {
//       if (dst_first == dst_last) break; else *(dst_first++) = '%';
//       if (dst_first == dst_last) break; else *(dst_first++) = value_to_hex1(*src_first);
//       if (dst_first == dst_last) break; else *(dst_first++) = value_to_hex0(*src_first);
//     }

//     ++src_first;
//   }

//   return dst_first;
// }

// template <typename SrcContainer, typename DestItr>
// DestItr
// copy_escape_html(const SrcContainer& src, DestItr dst_first, DestItr dst_last) {
//   return copy_escape_html(src.begin(), src.end(), dst_first, dst_last);
// }

// template <typename SrcContainer, typename DstContainer>
// typename DstContainer::iterator
// copy_escape_html(SrcContainer& src, DstContainer dst) {
//   return copy_escape_html(src.begin(), src.end(), dst.begin(), dst.end());
// }

// template <typename SrcItr, typename DstContainer>
// typename DstContainer::iterator
// copy_escape_html(SrcItr src_first, SrcItr src_last, DstContainer dst) {
//   return copy_escape_html(src_first, src_last, dst.begin(), dst.end());
// }


//
// Transform a sequence of bytes from/to a hex string.
//

template <typename SrcItr, typename DestItr>
DestItr
transform_from_hex(SrcItr src_first, SrcItr src_last, DestItr dst_first, DestItr dst_last) {
  auto dst_first_start = dst_first;

  while (src_first != src_last) {
    if (dst_first == dst_last)
      break;

    char high = hex_to_value_or_error(*src_first++);

    if (src_first == src_last)
      break;

    if (dst_first == dst_last || high == -1)
      return dst_first_start;

    char low = hex_to_value_or_error(*src_first++);

    if (dst_first == dst_last || low == -1)
      return dst_first_start;

    *dst_first++ = (high << 4) + low;
  }

  if (src_first != src_last)
    return dst_first_start;

  return dst_first;
}

template <typename SrcContainer, typename DestItr>
DestItr
transform_from_hex(const SrcContainer& src, DestItr dst_first, DestItr dst_last) {
  return transform_from_hex(src.begin(), src.end(), dst_first, dst_last);
}

template <typename SrcContainer, typename DstContainer>
typename DstContainer::iterator
transform_from_hex(const SrcContainer& src, DstContainer dst) {
  return transform_from_hex(src.begin(), src.end(), dst.begin(), dst.end());
}

template <typename SrcItr, typename DstContainer>
typename DstContainer::iterator
transform_from_hex(SrcItr src_first, SrcItr src_last, DstContainer dst) {
  return transform_from_hex(src_first, src_last, dst.begin(), dst.end());
}


template <typename SrcItr, typename DestItr>
DestItr
transform_to_hex(SrcItr src_first, SrcItr src_last, DestItr dst_first, DestItr dst_last) {
  if (std::distance(src_first, src_last) * 2 != std::distance(dst_first, dst_last))
    throw internal_error("transform_to_hex() incorrect destination size");

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

template <typename SrcContainer, typename DestItr>
DestItr
transform_to_hex(const SrcContainer& src, DestItr dst_first, DestItr dst_last) {
  return transform_to_hex(src.begin(), src.end(), dst_first, dst_last);
}

template <typename SrcContainer, typename DstContainer>
typename DstContainer::iterator
transform_to_hex(const SrcContainer& src, DstContainer dst) {
  return transform_to_hex(src.begin(), src.end(), dst.begin(), dst.end());
}

template <typename SrcItr, typename DstContainer>
typename DstContainer::iterator
transform_to_hex(SrcItr src_first, SrcItr src_last, DstContainer dst) {
  return transform_to_hex(src_first, src_last, dst.begin(), dst.end());
}

template <typename Container>
std::string
transform_to_hex_str(const Container& src) {
  return transform_to_hex_str(src.begin(), src.end());
}

template <typename SrcItr>
std::string
transform_to_hex_str(SrcItr src_first, SrcItr src_last) {
  std::string dest;
  dest.reserve(std::distance(src_first, src_last) * 2);

  while (src_first != src_last) {
    dest += value_to_hex1(*src_first);
    dest += value_to_hex0(*src_first);

    ++src_first;
  }

  return dest;
}

} // namespace torrent::utils

#endif // LIBTORRENT_TORRENT_UTILS_STRING_MANIP_H
