#include "config.h"

#include "torrent/net/http_get.h"

#include <cassert>

#include "net/curl_get.h"
#include "net/curl_stack.h"
#include "net/thread_net.h"
#include "torrent/exceptions.h"
#include "torrent/net/http_stack.h"
#include "torrent/utils/thread.h"

namespace torrent::net {

// TODO: Use Resolver for dns lookups?
// TODO: Make thread-safe, lock various parts after start.

// TODO: Start/close should set a bool in curl_get, which indicates we've added it to the callback queue.

HttpGet::HttpGet() = default;

HttpGet::HttpGet(const std::string& url, std::iostream* s) :
    m_curl_get(std::make_shared<CurlGet>()) {

  m_curl_get->set_url(url);
  m_curl_get->set_stream(s);
  m_curl_get->set_timeout(5 * 60); // Default to 5 minutes.
}

void
HttpGet::close() {
  auto curl_stack = m_curl_get->curl_stack();

  if (curl_stack != nullptr)
    curl_stack->close_get(m_curl_get);
}

std::string
HttpGet::url() const {
  return m_curl_get->url();
}

std::iostream*
HttpGet::stream() {
  return m_curl_get->stream();
}

uint32_t
HttpGet::timeout() const {
  return m_curl_get->timeout();
}

// TODO: Move the checks to CurlGet.

void
HttpGet::set_url(std::string url) {
  if (m_curl_get->is_busy())
    throw torrent::internal_error("Cannot set stream while HttpGet is busy.");

  m_curl_get->set_url(std::move(url));
}

void
HttpGet::set_stream(std::iostream* str) {
  if (m_curl_get->is_busy())
    throw torrent::internal_error("Cannot set stream while HttpGet is busy.");

  m_curl_get->set_stream(str);
}

void
HttpGet::set_timeout(uint32_t seconds) {
  if (m_curl_get->is_busy())
    throw torrent::internal_error("Cannot set timeout while HttpGet is busy.");

  m_curl_get->set_timeout(seconds);
}

int64_t
HttpGet::size_done() const {
  return m_curl_get->size_done();
}

int64_t
HttpGet::size_total() const {
  return m_curl_get->size_total();
}

// TODO: Wrap in callbacks.

void
HttpGet::add_done_slot(const std::function<void()>& slot) {
  m_curl_get->signal_done().push_back(slot);
}

void
HttpGet::add_failed_slot(const std::function<void(const std::string&)>& slot) {
  m_curl_get->signal_failed().push_back(slot);
}

}
