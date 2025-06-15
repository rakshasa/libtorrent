#include "config.h"

#include "net/curl_get.h"

#include <cassert>
#include <iostream>
#include <curl/easy.h>

#include "net/curl_stack.h"
#include "torrent/exceptions.h"
#include "utils/functional.h"

namespace {
size_t
curl_get_receive_write(const char* data, size_t size, size_t nmemb, torrent::net::CurlGet* handle) {
  if (!handle->stream()->write(data, size * nmemb).fail())
    return size * nmemb;
  else
    return 0;
}

} // namespace

namespace torrent::net {

CurlGet::CurlGet() {
  m_task_timeout.slot() = [this]() {
      trigger_failed("Timed out");
    };
}

CurlGet::~CurlGet() {
  // CurlStack keeps a shared_ptr to this object, so it will only be destroyed once it is removed
  // from the stack.
  assert(!is_stacked() && "CurlGet::~CurlGet called while still in the stack.");
}

void
CurlGet::set_url(const std::string& url) {
  auto guard = lock_guard();

  if (m_handle != nullptr)
    throw torrent::internal_error("CurlGet::set_url(...) called on a stacked object.");

  m_url = url;
}

// Make sure the output stream does not have any bad/failed bits set.
//
// TODO: Make the stream into something you pass to CurlGet, as a unique_ptr, and then have a way to
// receive it in a thread-safe way on success.
void
CurlGet::set_stream(std::iostream* str) {
  auto guard = lock_guard();

  if (m_handle != nullptr)
    throw torrent::internal_error("CurlGet::set_stream(...) called on a stacked object.");

  m_stream = str;
}

void
CurlGet::set_timeout(uint32_t seconds) {
  auto guard = lock_guard();

  if (m_handle != nullptr)
    throw torrent::internal_error("CurlGet::set_timeout(...) called on a stacked object.");

  m_timeout = seconds;
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

// TODO: When we add callback for start/close add an atomic_bool to indicate we've queued the
// action, and use that to tell the user that the http_get is busy or not.

// CurlStack locks CurlGet.
void
CurlGet::prepare_start_unsafe(CurlStack* stack) {
  if (m_handle != nullptr)
    throw torrent::internal_error("CurlGet::prepare_start(...) called on a stacked object.");

  if (m_stream == nullptr)
    throw torrent::internal_error("CurlGet::prepare_start(...) called with a null stream.");

  m_handle = curl_easy_init();
  m_stack = stack;

  if (m_handle == nullptr)
    throw torrent::internal_error("Call to curl_easy_init() failed.");

  curl_easy_setopt(m_handle, CURLOPT_URL,            m_url.c_str());
  curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION,  &curl_get_receive_write);
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

  m_ipv6 = false;
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

void
CurlGet::add_done_slot(const std::function<void()>& slot) {
  auto guard = lock_guard();

  if (m_handle != nullptr)
    throw torrent::internal_error("CurlGet::add_done_slot(...) called on a stacked object.");

  m_signal_done.push_back(slot);
}

void
CurlGet::add_failed_slot(const std::function<void(const std::string&)>& slot) {
  auto guard = lock_guard();

  if (m_handle != nullptr)
    throw torrent::internal_error("CurlGet::add_failed_slot(...) called on a stacked object.");

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

// TODO: Verify slots are handled while CurlGet and CurlStack are unlocked.

void
CurlGet::trigger_done() {
  auto signal_done = m_signal_done;

  ::utils::slot_list_call(signal_done);
}

void
CurlGet::trigger_failed(const std::string& message) {
  auto signal_failed = m_signal_failed;

  ::utils::slot_list_call(signal_failed, message);
}

} // namespace torrent::net
