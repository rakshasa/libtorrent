#include "config.h"

#include "torrent/utils/uri_parser.h"

#include <algorithm>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <sys/socket.h>

namespace torrent::utils {

static inline bool
is_unreserved_uri_char(char c) {
  return
    (c >= 'A' && c <= 'Z') ||
    (c >= 'a' && c <= 'z') ||
    (c >= '0' && c <= '9') ||
    c == '-' || c == '_' || c == '.' || c == '~';
}

static inline bool
is_valid_uri_query_char(char c) {
  return
    (c >= 'A' && c <= 'Z') ||
    (c >= 'a' && c <= 'z') ||
    (c >= '0' && c <= '9') ||
    c == '-' || c == '_' || c == '.' || c == '~' ||
    c == ':' || c == '&' || c == '=' || c == '/' ||
    c == '%';
}

static inline bool
is_unreserved_uri_query_char(char c) {
  return
    (c >= 'A' && c <= 'Z') ||
    (c >= 'a' && c <= 'z') ||
    (c >= '0' && c <= '9') ||
    c == '-' || c == '_' || c == '.' || c == '~' ||
    c == ':' || c == '=' || c == '/' || c == '%';
}

static inline bool
is_not_unreserved_uri_char(char c) {
  return !is_unreserved_uri_char(c);
}

static inline bool
is_not_valid_uri_query_char(char c) {
  return !is_valid_uri_query_char(c);
}

static inline bool
is_not_unreserved_uri_query_char(char c) {
  return !is_unreserved_uri_query_char(c);
}

template <int pos, typename Value>
static inline char
value_to_hexchar(Value v) {
  v >>= pos * 4;
  v &= 0xf;

  if (v < 0xA)
    return '0' + v;
  else
    return 'A' + v - 0xA;
}

template<typename Ftor>
static inline std::string::const_iterator
uri_string_copy_until(std::string::const_iterator first, std::string::const_iterator last,
                      std::string& result, Ftor check) {
  std::string::const_iterator next = std::find_if(first, last, check);

  result = std::string(first, next);
  return next;
}

static inline void
uri_parse_throw_error(const char* error_msg, char invalid_char) {
  std::string error_str = std::string(error_msg);
  error_str += value_to_hexchar<1>(invalid_char);
  error_str += value_to_hexchar<0>(invalid_char);

  throw uri_error(error_str);
}

void
uri_parse_str(std::string uri, uri_state& state) {
  if (state.state != uri_state::state_empty)
    throw uri_error("uri_state.state is not uri_state::state_empty");

  state.uri.swap(uri);
  state.state = uri_state::state_invalid;

  std::string::const_iterator first = state.uri.begin();
  std::string::const_iterator last = state.uri.end();

  // Parse scheme:
  first = uri_string_copy_until(first, last, state.scheme, &is_not_unreserved_uri_char);

  if (first == last)
    goto uri_parse_success;

  if (*first++ != ':')
    uri_parse_throw_error("could not find ':' after scheme, found character 0x", *--first);

  // Parse resource:
  first = uri_string_copy_until(first, last, state.resource, &is_not_unreserved_uri_char);

  if (first == last)
    goto uri_parse_success;

  if (*first++ != '?')
    uri_parse_throw_error("could not find '?' after resource, found character 0x", *--first);

  // Parse query:
  first = uri_string_copy_until(first, last, state.query, &is_not_valid_uri_query_char);

  if (first == last)
    goto uri_parse_success;

  if (*first++ != '#')
    uri_parse_throw_error("could not find '#' after query, found character 0x", *--first);

 uri_parse_success:
  state.state = uri_state::state_valid;
  return;
}

void
uri_parse_c_str(const char* uri, uri_state& state) {
  uri_parse_str(std::string(uri), state);
}

// * Letters (A-Z and a-z), numbers (0-9) and the characters
//   '.','-','~' and '_' are left as-is
// * SPACE is encoded as '+' or "%20"
// * All other characters are encoded as %HH hex representation with
//   any non-ASCII characters first encoded as UTF-8 (or other
//   specified encoding)
void
uri_parse_query_str(std::string query, uri_query_state& state) {
  if (state.state != uri_query_state::state_empty)
    throw uri_error("uri_query_state.state is not uri_query_state::state_empty");

  state.query.swap(query);
  state.state = uri_state::state_invalid;

  std::string::const_iterator first = state.query.begin();
  std::string::const_iterator last = state.query.end();

  while (first != last) {
    std::string element;
    first = uri_string_copy_until(first, last, element, &is_not_unreserved_uri_query_char);

    if (first != last && *first++ != '&') {
      std::string invalid_hex;
      invalid_hex += value_to_hexchar<1>(*--first);
      invalid_hex += value_to_hexchar<0>(*first);

      throw uri_error("query element contains invalid character 0x" + invalid_hex);
    }

    state.elements.push_back(element);
  }

  state.state = uri_state::state_valid;
}

std::string
uri_generate_scrape_url(std::string uri) {
  size_t delim_slash = uri.rfind('/');

  if (delim_slash == std::string::npos || uri.find("/announce", delim_slash) != delim_slash)
    throw input_error("Tried to make scrape url from invalid uri.");

  return uri.replace(delim_slash, sizeof("/announce") - 1, "/scrape");
}

bool
uri_can_scrape(const std::string& uri) {
  // TODO: Replace with uri parsers above.

  size_t delim_slash = uri.rfind('/');

  if (delim_slash == std::string::npos)
    return false;

  // TODO: This should be more robust.
  return uri.find("/announce", delim_slash) == delim_slash;
}

bool
uri_has_query(const std::string& uri) {
  // TODO: Replace with uri parsers above.

  size_t delim_options = uri.rfind('?');

  if (delim_options == std::string::npos)
    return false;

  return uri.find('/', delim_options) == std::string::npos;
}

int
uri_detect_numeric(const std::string& uri) {
  CURLU *url = curl_url();
  char *host;

  if (curl_url_set(url, CURLUPART_URL, uri.c_str(), 0) != CURLUE_OK) {
    curl_url_cleanup(url);
    return AF_UNSPEC;
  }

  if (curl_url_get(url, CURLUPART_HOST, &host, 0) != CURLUE_OK) {
    curl_url_cleanup(url);
    return AF_UNSPEC;
  }

  char   buf[16];
  int    family = AF_UNSPEC;
  size_t host_len = strlen(host);

  if (host_len >= 2 && host[0] == '[' && host[host_len - 1] == ']') {
    host[host_len - 1] = '\0';

    if (::inet_pton(AF_INET6, host + 1, buf) == 1)
      family = AF_INET6;

  } else {
    if (::inet_pton(AF_INET, host, buf) == 1)
      family = AF_INET;
  }

  curl_free(host);
  curl_url_cleanup(url);
  return family;
}

std::string
uri_escape_html(const char* first, const char* last) {
  std::string result;
  result.reserve(std::distance(first, last) * 3);

  while (first != last) {
    if ((*first >= 'A' && *first <= 'Z') ||
        (*first >= 'a' && *first <= 'z') ||
        (*first >= '0' && *first <= '9') ||
        *first == '-') {
      result.push_back(*first);

    } else {
      result.push_back('%');
      result.push_back(value_to_hexchar<1>(*first));
      result.push_back(value_to_hexchar<0>(*first));
    }

    ++first;
  }

  return result;
}

} // namespace torrent::utils
