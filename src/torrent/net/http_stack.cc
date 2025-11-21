#include "config.h"

#include "torrent/net/http_stack.h"

#include <cassert>

#include "net/curl_get.h"
#include "net/curl_stack.h"
#include "net/thread_net.h"
#include "torrent/exceptions.h"
#include "torrent/net/http_get.h"
#include "torrent/utils/thread.h"

namespace torrent::net {

HttpStack::HttpStack(utils::Thread* thread) :
    m_stack(new CurlStack(thread)) {
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
