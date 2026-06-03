#include "config.h"

#include "torrent/utils/string_manip.h"

#include <algorithm>
#include <exception>
#include <openssl/evp.h>

#include "torrent/common.h"

namespace torrent::utils {

bool
is_valid_utf8(const std::string& str) {
  auto itr       = str.begin();
  const auto end = str.end();

  while (itr != end) {
    int  num{};
    auto byte = static_cast<unsigned char>(*itr);

    // Verify leading byte.
    if ((byte & 0x80) == 0x00)
      num = 1;
    else if ((byte & 0xE0) == 0xC0)
      num = 2;
    else if ((byte & 0xF0) == 0xE0)
      num = 3;
    else if ((byte & 0xF8) == 0xF0)
      num = 4;
    else
      return false;

    ++itr;

    // Check continuation bytes.
    for (int i = 1; i < num; ++i) {
      if (itr == end)
        return false;

      if ((static_cast<unsigned char>(*itr) & 0xC0) != 0x80)
        return false;

      ++itr;
    }
  }

  return true;
}

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

std::string
transform_to_base64(const std::string& src) {
  if (src.empty()) return {};

  std::string result((4 * ((src.size() + 2) / 3)), '\0');

  int actual_length = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(result.data()),
                                      reinterpret_cast<const unsigned char*>(src.data()),
                                      src.size());

  result.resize(actual_length);
  return result;
}

std::optional<std::vector<uint8_t>>
transform_from_base64_unsafe(const std::string& src) {
  if (src.empty())
    return std::vector<uint8_t>{};

  if (src.length() % 4)
    return std::nullopt;

  std::vector<uint8_t> bytes((src.length() * 3) / 4);

  int decoded_len = EVP_DecodeBlock(bytes.data(), reinterpret_cast<const uint8_t*>(src.data()), src.length());

  if (decoded_len <= 0)
    return std::nullopt;

  if (src.back() == '=')
    decoded_len--;

  if (src.length() > 1 && src[src.length() - 2] == '=')
    decoded_len--;

  // If the input contains extra padding characters, this could cause negative decoded_len.
  if (decoded_len < 0)
    return std::nullopt;

  bytes.resize(decoded_len);

  return bytes;
}

} // namespace torrent::utils
