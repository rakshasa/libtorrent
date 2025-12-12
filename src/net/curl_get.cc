#include "config.h"

#include "net/curl_get.h"

#include <cassert>
#include <iostream>
#include <utility>
#include <curl/easy.h>

#include "net/curl_stack.h"
#include "torrent/exceptions.h"
#include "torrent/net/network_config.h"
#include "torrent/net/socket_address.h"
#include "torrent/utils/thread.h"
#include "torrent/utils/uri_parser.h"
#include "utils/functional.h"

namespace torrent::net {

CurlGet::CurlGet(std::string url, std::shared_ptr<std::ostream> stream)
  : m_url(std::move(url)),
    m_stream(std::move(stream)) {
}

CurlGet::~CurlGet() {
  // CurlStack keeps a shared_ptr to this object, so it will only be destroyed once it is removed
  // from the stack.
  assert(!is_stacked() && "CurlGet::~CurlGet called while still in the stack.");
}

int64_t
CurlGet::size_done() {
  auto guard = lock_guard();

  if (m_handle == nullptr)
    return 0;

  curl_off_t d = 0;
  curl_easy_getinfo(m_handle, CURLINFO_SIZE_DOWNLOAD_T, &d);

  return d;
}

int64_t
CurlGet::size_total() {
  auto guard = lock_guard();

  if (m_handle == nullptr)
    return 0;

  curl_off_t d = 0;
  curl_easy_getinfo(m_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &d);

  return d;
}

void
CurlGet::reset(const std::string& url, std::shared_ptr<std::ostream> stream) {
  auto guard = lock_guard();

  if (m_was_started)
    throw torrent::internal_error("CurlGet::reset() called on a started object.");

  if (m_handle != nullptr)
    throw torrent::internal_error("CurlGet::reset() called on a stacked object.");

  m_url    = url;
  m_stream = std::move(stream);
}

void
CurlGet::wait_for_close() {
  std::unique_lock<std::mutex> guard(m_mutex);

  if (!m_was_closed)
    throw torrent::internal_error("CurlGet::wait_for_close() called on an object that is not closing.");

  if (m_handle != nullptr)
    m_cond_closed.wait(guard, [this] { return m_handle == nullptr; });
}

bool
CurlGet::try_wait_for_close() {
  std::unique_lock<std::mutex> guard(m_mutex);

  if (!m_was_closed)
    return false;

  if (m_handle != nullptr)
    m_cond_closed.wait(guard, [this] { return m_handle == nullptr; });

  return true;
}

void
CurlGet::set_timeout(uint32_t seconds) {
  auto guard = lock_guard();

  if (m_was_started)
    throw torrent::internal_error("CurlGet::set_timeout(...) called on a started object.");

  m_timeout = seconds;
}

void
CurlGet::set_initial_resolve(resolve_type type) {
  auto guard = lock_guard();

  if (m_was_started)
    throw torrent::internal_error("CurlGet::set_initial_resolve(...) called on a stacked or started object.");

  if (type == RESOLVE_NONE)
    throw torrent::internal_error("CurlGet::set_initial_resolve(...) called with RESOLVE_NONE, which is not allowed.");

  m_initial_resolve = type;
}

void
CurlGet::set_retry_resolve(resolve_type type) {
  auto guard = lock_guard();

  if (m_was_started)
    throw torrent::internal_error("CurlGet::set_retry_resolve(...) called on a stacked or started object.");

  m_retry_resolve = type;
}

void
CurlGet::start(const std::shared_ptr<CurlGet>& curl_get, CurlStack* stack) {
  auto self = curl_get.get();

  std::unique_lock<std::mutex> guard(self->m_mutex);

  if (self->m_was_started)
    throw torrent::internal_error("CurlGet::start() called on an already started object.");

  self->m_was_started  = true;
  self->m_stack_thread = stack->thread();

  stack->thread()->callback(self, [stack, curl_get]() {
      stack->start_get(curl_get);
    });
}

void
CurlGet::close(const std::shared_ptr<CurlGet>& curl_get, utils::Thread* callback_thread, bool wait) {
  auto self = curl_get.get();

  std::unique_lock<std::mutex> guard(self->m_mutex);

  if (!self->m_was_started)
    throw torrent::internal_error("CurlGet::close_and_cancel_callbacks() called on an object that was not started.");

  if (self->m_was_closed)
    throw torrent::internal_error("CurlGet::close_and_cancel_callbacks() called on an already closing object.");

  self->m_stack_thread->cancel_callback(self);

  if (callback_thread != nullptr)
    callback_thread->cancel_callback(self);

  if (self->m_stack == nullptr) {
    if (self->m_was_started) {
      self->m_stack_thread     = nullptr;
      self->m_was_started      = false;
      self->m_was_closed       = false;
      self->m_prepare_canceled = false;
      self->m_retrying_resolve = false;
    }

    return;
  }

  assert(std::this_thread::get_id() != self->m_stack->thread()->thread_id());
  assert(callback_thread != self->m_stack->thread());

  self->m_was_closed = true;

  self->m_stack->thread()->callback_interrupt_pollling(self, [curl_stack = self->m_stack, curl_get]() {
      curl_stack->close_get(curl_get);
    });

  if (wait && self->m_handle == nullptr)
    self->m_cond_closed.wait(guard, [self] { return self->m_handle == nullptr; });
}

bool
CurlGet::prepare_start_unsafe(CurlStack* stack) {
  if (m_handle != nullptr)
    throw torrent::internal_error("CurlGet::prepare_start(...) called on a stacked object.");

  if (m_stream == nullptr)
    throw torrent::internal_error("CurlGet::prepare_start(...) called with a null stream.");

  if (!m_was_started)
    throw torrent::internal_error("CurlGet::prepare_start(...) called on an object that was not started.");

  if (m_was_closed) {
    m_prepare_canceled = true;
    return false;
  }

  m_handle = curl_easy_init();
  m_stack = stack;
  m_retrying_resolve = false;

  if (m_handle == nullptr)
    throw torrent::internal_error("Call to curl_easy_init() failed.");

  curl_easy_setopt(m_handle, CURLOPT_URL,            m_url.c_str());
  curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION,  &CurlGet::receive_write);
  curl_easy_setopt(m_handle, CURLOPT_WRITEDATA,      this);

  if (m_timeout != 0) {
    curl_easy_setopt(m_handle, CURLOPT_CONNECTTIMEOUT, 60l);
    curl_easy_setopt(m_handle, CURLOPT_TIMEOUT,        static_cast<long>(m_timeout));
  }

  curl_easy_setopt(m_handle, CURLOPT_NOSIGNAL,       1l);
  curl_easy_setopt(m_handle, CURLOPT_FOLLOWLOCATION, 1l);
  curl_easy_setopt(m_handle, CURLOPT_MAXREDIRS,      5l);
  curl_easy_setopt(m_handle, CURLOPT_ENCODING,       "");

  // Note that if the url has a numeric IP address, libcurl will not respect the CURLOPT_IPRESOLVE
  // option.
  //
  // We need to make CurlGet fail on numeric urls which do not match the resolve type.

  if (m_initial_resolve == RESOLVE_NONE)
    throw torrent::internal_error("CurlGet::prepare_start_unsafe() called with m_initial_resolve set to RESOLVE_NONE.");

  if (m_initial_resolve == RESOLVE_WHATEVER) {
    curl_easy_setopt(m_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_WHATEVER);
    return true;
  }

  try {
    try {
      if (prepare_resolve(m_initial_resolve))
        return true;

    } catch (const torrent::input_error& e) {
      if (m_retry_resolve == RESOLVE_NONE)
        throw;

      // TODO: Save the error msg for later.
    }

    m_retrying_resolve = true;

    if (prepare_resolve(m_retry_resolve))
      return true;

    throw torrent::input_error("Exhausted all resolve options.");

  } catch (const torrent::input_error& e) {
    m_prepare_canceled = true;
    ::utils::slot_list_call(m_signal_failed, e.what());
    return false;
  }
}

void
CurlGet::activate_unsafe() {
  CURLMcode code = curl_multi_add_handle(m_stack->handle(), m_handle);

  if (code != CURLM_OK)
    throw torrent::internal_error("CurlGet::activate() error calling curl_multi_add_handle: " + std::string(curl_multi_strerror(code)));

  m_active = true;
}

void
CurlGet::cleanup_unsafe() {
  if (m_handle != nullptr) {
    if (m_active) {
      CURLMcode code = curl_multi_remove_handle(m_stack->handle(), m_handle);

      if (code != CURLM_OK)
        throw torrent::internal_error("CurlGet::cleanup() error calling curl_multi_remove_handle: " + std::string(curl_multi_strerror(code)));

      m_active = false;
    }

    curl_easy_cleanup(m_handle);

    m_handle = nullptr;
    m_stack  = nullptr;

  } else {
    if (!m_prepare_canceled)
      throw torrent::internal_error("CurlGet::cleanup() called on an object that has no m_handle yet m_prepare_canceled is false.");
  }

  m_stack_thread     = nullptr;
  m_was_started      = false;
  m_was_closed       = false;
  m_prepare_canceled = false;
  m_retrying_resolve = false;
}

// Slots can be added at any time, however once trigger_* is called, it will make a copy of the
// current list.
void
CurlGet::add_done_slot(const std::function<void()>& slot) {
  auto guard = lock_guard();
  m_signal_done.push_back(slot);
}

void
CurlGet::add_failed_slot(const std::function<void(const std::string&)>& slot) {
  auto guard = lock_guard();
  m_signal_failed.push_back(slot);
}

bool
CurlGet::retry_resolve() {
  auto guard = lock_guard();

  if (m_handle == nullptr)
    throw torrent::internal_error("CurlGet::retry_ipv6() called on a null m_handle.");

  if (m_retrying_resolve || m_retry_resolve == RESOLVE_NONE)
    return false;

  // TODO: Is this correct if we fail?
  CURLMcode code = curl_multi_remove_handle(m_stack->handle(), m_handle);

  if (code != CURLM_OK)
    throw torrent::internal_error("CurlGet::cleanup() error calling curl_multi_remove_handle: " + std::string(curl_multi_strerror(code)));

  m_retrying_resolve = true;

  try {
    return prepare_resolve(m_retry_resolve);

  } catch (const torrent::input_error& e) {
    // TODO: Consider adding to error message.
    return false;
  }
}

void
CurlGet::trigger_done() {
  auto guard = lock_guard();

  if (m_was_closed)
    return;

  ::utils::slot_list_call(m_signal_done);
}

void
CurlGet::trigger_failed(const std::string& message) {
  auto guard = lock_guard();

  if (m_was_closed)
    return;

  // If this is changed, verify RESOLVE_NONE handling is correct.
  ::utils::slot_list_call(m_signal_failed, message);
}

size_t
CurlGet::receive_write(const char* data, size_t size, size_t nmemb, CurlGet* handle) {
  if (handle->m_stream->write(data, size * nmemb).fail())
    return 0;

  return size * nmemb;
}

bool
CurlGet::prepare_resolve(resolve_type current_resolve) {
  auto [bind_inet_address, bind_inet6_address] = config::network_config()->bind_addresses_or_null();

  int detected_family = utils::uri_detect_numeric(m_url);

  switch (current_resolve) {
  case RESOLVE_IPV4:
    if (bind_inet_address == nullptr)
      throw torrent::input_error("Bind address for requested IP protocol(s) not available.");

    if (detected_family == AF_INET6)
      throw torrent::input_error("Numeric IPv6 address in url, but IPv4 was requested.");

    if (bind_inet_address->sa_family != AF_UNSPEC)
      curl_easy_setopt(m_handle, CURLOPT_INTERFACE, sa_addr_str(bind_inet_address.get()).c_str());

    curl_easy_setopt(m_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    return true;

  case RESOLVE_IPV6:
    if (bind_inet6_address == nullptr)
      throw torrent::input_error("Bind address for requested IP protocol(s) not available.");

    if (detected_family == AF_INET)
      throw torrent::input_error("Numeric IPv4 address in url, but IPv6 was requested.");

    if (bind_inet6_address->sa_family != AF_UNSPEC)
      curl_easy_setopt(m_handle, CURLOPT_INTERFACE, sa_addr_str(bind_inet6_address.get()).c_str());

    curl_easy_setopt(m_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
    return true;

  case RESOLVE_NONE:
    return false;

  default:
    throw torrent::internal_error("CurlGet::prepare_start_unsafe() reached unreachable code with invalid current_resolve.");
  }
}

} // namespace torrent::net
