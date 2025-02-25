#ifndef LIBTORRENT_UTILS_URI_PARSER_H
#define LIBTORRENT_UTILS_URI_PARSER_H

#include <string>
#include <vector>
#include <torrent/common.h>
#include <torrent/exceptions.h>

namespace torrent::utils {

typedef std::vector<std::string> uri_resource_list;
typedef std::vector<std::string> uri_query_list;

struct uri_base_state {
  static const int state_empty = 0;
  static const int state_valid = 1;
  static const int state_invalid = 2;

  uri_base_state() : state(state_empty) {}

  int state;
};

struct uri_state : uri_base_state {
  std::string uri;
  std::string scheme;
  std::string resource;
  std::string query;
  std::string fragment;
};

struct uri_resource_state : public uri_base_state {
  std::string resource;
  uri_resource_list path;
};

struct uri_query_state : public uri_base_state {
  std::string query;
  uri_query_list elements;
};

class uri_error : public ::torrent::input_error {
public:
  uri_error(const char* msg) : ::torrent::input_error(msg) {}
  uri_error(const std::string& msg) : ::torrent::input_error(msg) {}
};

void uri_parse_str(std::string uri, uri_state& state) LIBTORRENT_EXPORT;
void uri_parse_c_str(const char* uri, uri_state& state) LIBTORRENT_EXPORT;

void uri_parse_resource(std::string query, uri_query_state& state) LIBTORRENT_EXPORT;
void uri_parse_resource_authority(std::string query, uri_query_state& state) LIBTORRENT_EXPORT;
void uri_parse_resource_path(std::string query, uri_query_state& state) LIBTORRENT_EXPORT;

void uri_parse_query_str(std::string query, uri_query_state& state) LIBTORRENT_EXPORT;
void uri_parse_query_c_str(const char* query, uri_query_state& state) LIBTORRENT_EXPORT;

std::string uri_generate_scrape_url(std::string uri) LIBTORRENT_EXPORT;

bool uri_can_scrape(const std::string& uri) LIBTORRENT_EXPORT;
bool uri_has_query(const std::string& uri) LIBTORRENT_EXPORT;

} // namespace torrent::utils

#endif
