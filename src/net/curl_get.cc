#include "config.h"

#include "net/curl_get.h"

#include <cassert>
#include <iostream>
#include <curl/easy.h>

#include "net/curl_stack.h"
#include "torrent/exceptions.h"
#include "utils/functional.h"

namespace torrent::net {

CurlGet::CurlGet(const std::string& url, std::shared_ptr<std::ostream> stream) :
    m_url(url),
    m_stream(std::move(stream)) {

  m_task_timeout.slot() = [this]() {
      // TODO: Should make sure it closes/disables further callbacks.
      trigger_failed("Timed out");
    };
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

  if (m_handle != nullptr)
    throw torrent::internal_error("CurlGet::set_url(...) called on a stacked object.");

  m_url = url;
  m_stream = std::move(stream);
  m_was_started = false;
  m_was_closed = false;
  m_ipv6 = false;
}

void
CurlGet::set_timeout(uint32_t seconds) {
  auto guard = lock_guard();

  if (m_handle != nullptr || m_was_started)
    throw torrent::internal_error("CurlGet::set_timeout(...) called on a stacked or started object.");

  m_timeout = seconds;
}

void
CurlGet::set_was_started() {
  auto guard = lock_guard();

  if (m_handle != nullptr || m_was_started)
    throw torrent::internal_error("CurlGet::set_was_started() called on a stacked or started object.");

  m_was_started = true;
}

bool
CurlGet::set_was_closed() {
  auto guard = lock_guard();

  if (m_was_closed)
    throw torrent::internal_error("CurlGet::set_was_closed() called on a closed object.");

  if (!m_was_started)
    return false;

  m_was_closed = true;
  return true;
}

bool
CurlGet::prepare_start_unsafe(CurlStack* stack) {
  if (m_handle != nullptr)
    throw torrent::internal_error("CurlGet::prepare_start(...) called on a stacked object.");

  if (m_stream == nullptr)
    throw torrent::internal_error("CurlGet::prepare_start(...) called with a null stream.");

  if (!m_was_started)
    throw torrent::internal_error("CurlGet::prepare_start(...) called on an object that was not started.");

  if (m_was_closed)
    return false;

  m_handle = curl_easy_init();
  m_stack = stack;
  m_ipv6 = false;

  if (m_handle == nullptr)
    throw torrent::internal_error("Call to curl_easy_init() failed.");

  curl_easy_setopt(m_handle, CURLOPT_URL,            m_url.c_str());
  curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION,  &CurlGet::receive_write);
  curl_easy_setopt(m_handle, CURLOPT_WRITEDATA,      this);

  if (m_timeout != 0) {
    curl_easy_setopt(m_handle, CURLOPT_CONNECTTIMEOUT, 60l);
    curl_easy_setopt(m_handle, CURLOPT_TIMEOUT,        static_cast<long>(m_timeout));
  }

  curl_easy_setopt(m_handle, CURLOPT_FORBID_REUSE,   1l);
  curl_easy_setopt(m_handle, CURLOPT_NOSIGNAL,       1l);
  curl_easy_setopt(m_handle, CURLOPT_FOLLOWLOCATION, 1l);
  curl_easy_setopt(m_handle, CURLOPT_MAXREDIRS,      5l);

  curl_easy_setopt(m_handle, CURLOPT_IPRESOLVE,      CURL_IPRESOLVE_WHATEVER);
  curl_easy_setopt(m_handle, CURLOPT_ENCODING,       "");

  return true;
}

void
CurlGet::activate_unsafe() {
  CURLMcode code = curl_multi_add_handle(m_stack->handle(), m_handle);

  if (code != CURLM_OK)
    throw torrent::internal_error("CurlGet::activate() error calling curl_multi_add_handle: " + std::string(curl_multi_strerror(code)));

  // Normally libcurl should handle the timeout. But sometimes that doesn't
  // work right so we do a fallback timeout that just aborts the transfer.
  if (m_timeout != 0)
    torrent::this_thread::scheduler()->update_wait_for_ceil_seconds(&m_task_timeout, 20s + 1s*m_timeout);

  m_active = true;
}

void
CurlGet::cleanup_unsafe() {
  if (m_handle == nullptr)
    throw torrent::internal_error("CurlGet::cleanup() called on a null m_handle.");

  if (m_active) {
    CURLMcode code = curl_multi_remove_handle(m_stack->handle(), m_handle);

    if (code != CURLM_OK)
      throw torrent::internal_error("CurlGet::cleanup() error calling curl_multi_remove_handle: " + std::string(curl_multi_strerror(code)));

    torrent::this_thread::scheduler()->erase(&m_task_timeout);
    m_active = false;
  }

  curl_easy_cleanup(m_handle);

  m_handle = nullptr;
  m_stack = nullptr;
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

void
CurlGet::retry_ipv6() {
  auto guard = lock_guard();

  if (m_handle == nullptr)
    throw torrent::internal_error("CurlGet::retry_ipv6() called on a null m_handle.");

  CURL* new_handle = curl_easy_duphandle(m_handle);

  curl_easy_setopt(new_handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
  curl_easy_cleanup(m_handle);

  m_handle = new_handle;
  m_ipv6 = true;
}

void
CurlGet::trigger_done() {
  auto guard = lock_guard();

  torrent::this_thread::scheduler()->erase(&m_task_timeout);

  if (m_was_closed)
    return;

  ::utils::slot_list_call(m_signal_done);
}

void
CurlGet::trigger_failed(const std::string& message) {
  auto guard = lock_guard();

  torrent::this_thread::scheduler()->erase(&m_task_timeout);

  if (m_was_closed)
    return;

  ::utils::slot_list_call(m_signal_failed, message);
}

size_t
CurlGet::receive_write(const char* data, size_t size, size_t nmemb, CurlGet* handle) {
  if (handle->m_stream->write(data, size * nmemb).fail())
    return 0;

  return size * nmemb;
}


} // namespace torrent::net
