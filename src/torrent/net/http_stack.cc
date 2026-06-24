#include "config.h"

#include "torrent/net/http_stack.h"

#include <cassert>
#include <charconv>
#include <curl/curl.h>

#include "net/curl_get.h"
#include "net/curl_stack.h"
#include "net/thread_net.h"
#include "torrent/exceptions.h"
#include "torrent/net/http_get.h"
#include "torrent/system/thread.h"

namespace torrent::net {

// TODO: This should be in a net/utils file.
// TODO: Require scheme to also be returned / checked?

bool
verify_url_guess_scheme(const std::string& url) {
  auto curlu = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>(curl_url(), &curl_url_cleanup);

  return curl_url_set(curlu.get(), CURLUPART_URL, url.c_str(), CURLU_GUESS_SCHEME) == CURLUE_OK;
}

bool
verify_no_path_query_fragment(const std::string& url) {
  auto curlu = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>(curl_url(), &curl_url_cleanup);

  if (curl_url_set(curlu.get(), CURLUPART_URL, url.c_str(), CURLU_NON_SUPPORT_SCHEME) != CURLUE_OK)
    return false;

  char* path_ptr{};
  char* query_ptr{};
  char* fragment_ptr{};

  if (curl_url_get(curlu.get(), CURLUPART_PATH, &path_ptr, 0) == CURLUE_OK) {
    curl_free(path_ptr);
    return false;
  }

  if (curl_url_get(curlu.get(), CURLUPART_QUERY, &query_ptr, 0) == CURLUE_OK) {
    curl_free(query_ptr);
    return false;
  }

  if (curl_url_get(curlu.get(), CURLUPART_FRAGMENT, &fragment_ptr, 0) == CURLUE_OK) {
    curl_free(fragment_ptr);
    return false;
  }

  return true;
}

std::string
parse_uri_scheme(const std::string& url) {
  auto curlu = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>(curl_url(), &curl_url_cleanup);

  if (curl_url_set(curlu.get(), CURLUPART_URL, url.c_str(), CURLU_NON_SUPPORT_SCHEME) != CURLUE_OK)
    return {};

  char* scheme_ptr{};

  if (curl_url_get(curlu.get(), CURLUPART_SCHEME, &scheme_ptr, 0) != CURLUE_OK)
    return {};

  std::string scheme(scheme_ptr);
  curl_free(scheme_ptr);

  return scheme;
}

std::pair<std::string, uint16_t>
parse_uri_host_port(const std::string& uri) {
  auto curlu = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>(curl_url(), &curl_url_cleanup);

  if (curl_url_set(curlu.get(), CURLUPART_URL, uri.c_str(), CURLU_NON_SUPPORT_SCHEME) != CURLUE_OK)
    return {"", 0};

  char* host_ptr{};
  char* port_ptr{};

  if (curl_url_get(curlu.get(), CURLUPART_HOST, &host_ptr, 0) != CURLUE_OK)
    return {"", 0};

  std::string host(host_ptr);
  curl_free(host_ptr);

  if (curl_url_get(curlu.get(), CURLUPART_PORT, &port_ptr, 0) != CURLUE_OK)
    return {host, 0};

  uint16_t port{};

  auto result = std::from_chars(port_ptr, port_ptr + std::strlen(port_ptr), port);
  curl_free(port_ptr);

  if (result.ec != std::errc())
     return {"", 0};

  return {host, port};
}

std::pair<std::string, std::string>
parse_uri_user_password(const std::string& uri) {
  auto curlu = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>(curl_url(), &curl_url_cleanup);

  if (curl_url_set(curlu.get(), CURLUPART_URL, uri.c_str(), CURLU_NON_SUPPORT_SCHEME) != CURLUE_OK)
    return {"", ""};

  char* user_ptr{};
  char* password_ptr{};

  if (curl_url_get(curlu.get(), CURLUPART_USER, &user_ptr, 0) != CURLUE_OK)
    return {"", ""};

  std::string user(user_ptr);
  curl_free(user_ptr);

  if (curl_url_get(curlu.get(), CURLUPART_PASSWORD, &password_ptr, 0) != CURLUE_OK)
    return {user, ""};

  std::string password(password_ptr);
  curl_free(password_ptr);

  return {user, password};
}

HttpStack::HttpStack(system::Thread* thread)
  : m_stack(new CurlStack(thread)) {
}

HttpStack::~HttpStack() = default;

void
HttpStack::shutdown() {
  m_stack->shutdown();
}

void
HttpStack::start_get(HttpGet& http_get) {
  if (!http_get.is_valid())
    throw torrent::internal_error("HttpStack::start_get() called with an invalid HttpGet object.");

  CurlGet::start(http_get.curl_get(), m_stack.get());
}

unsigned int
HttpStack::size() const {
  return m_stack->size();
}

unsigned int
HttpStack::max_cache_connections() const {
  return m_stack->max_cache_connections();
}

unsigned int
HttpStack::max_host_connections() const {
  return m_stack->max_host_connections();
}

unsigned int
HttpStack::max_total_connections() const {
  return m_stack->max_total_connections();
}

void
HttpStack::set_max_cache_connections(unsigned int value) {
  m_stack->set_max_cache_connections(value);
}

void
HttpStack::set_max_host_connections(unsigned int value) {
  m_stack->set_max_host_connections(value);
}

void
HttpStack::set_max_total_connections(unsigned int value) {
  m_stack->set_max_total_connections(value);
}

std::string
HttpStack::user_agent() const {
  return m_stack->user_agent();
}

std::string
HttpStack::http_proxy() const {
  return m_stack->http_proxy();
}

std::string
HttpStack::http_capath() const {
  return m_stack->http_capath();
}

std::string
HttpStack::http_cacert() const {
  return m_stack->http_cacert();
}

void
HttpStack::set_user_agent(const std::string& s) {
  m_stack->set_user_agent(s);
}

void
HttpStack::set_http_proxy(const std::string& s) {
  if (!s.empty() && !verify_url_guess_scheme(s))
    throw torrent::input_error("Invalid HTTP proxy url: " + s);

  m_stack->set_http_proxy(s);
}

void
HttpStack::set_http_capath(const std::string& s) {
  m_stack->set_http_capath(s);
}

void
HttpStack::set_http_cacert(const std::string& s) {
  m_stack->set_http_cacert(s);
}

bool
HttpStack::ssl_verify_host() const {
  return m_stack->ssl_verify_host();
}

bool
HttpStack::ssl_verify_peer() const {
  return m_stack->ssl_verify_peer();
}

void
HttpStack::set_ssl_verify_host(bool s) {
  m_stack->set_ssl_verify_host(s);
}

void
HttpStack::set_ssl_verify_peer(bool s) {
  m_stack->set_ssl_verify_peer(s);
}

long
HttpStack::dns_timeout() const {
  return m_stack->dns_timeout();
}

void
HttpStack::set_dns_timeout(long timeout) {
  m_stack->set_dns_timeout(timeout);
}

} // namespace torrent::net
