#include "config.h"

#include "net/curl_get.h"

#include <cassert>
#include <iostream>
#include <curl/easy.h>

#include "net/curl_stack.h"
#include "torrent/exceptions.h"
#include "utils/functional.h"

namespace torrent::net {

// TODO: Seperate the thread-owned and public variables in different cachelines.

static size_t
curl_get_receive_write(void* data, size_t size, size_t nmemb, void* handle) {
  if (!((CurlGet*)handle)->stream()->write((const char*)data, size * nmemb).fail())
    return size * nmemb;
  else
    return 0;
}

CurlGet::CurlGet() {
  m_task_timeout.slot() = [this]() { receive_timeout(); };
}

CurlGet::~CurlGet() {
  // CurlStack keeps a shared_ptr to this object, so it will only be destroyed once it is removed
  // from the stack.
  assert(!is_busy() && "CurlGet::~CurlGet called while still busy.");
}

void
CurlGet::set_url(const std::string& url) {
  auto guard = lock_guard();

  if (m_handle != nullptr)
    throw torrent::internal_error("CurlGet::set_url(...) called on a busy object.");

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
    throw torrent::internal_error("CurlGet::set_stream(...) called on a busy object.");

  m_stream = str;
}

void
CurlGet::set_timeout(uint32_t seconds) {
  auto guard = lock_guard();

  if (m_handle != nullptr)
    throw torrent::internal_error("CurlGet::set_timeout(...) called on a busy object.");

  m_timeout = seconds;
}

// TODO: When we add callback for start/close add an atomic_bool to indicate we've queued the
// action, and use that to tell the user that the http_get is busy or not.

void
CurlGet::prepare_start(CurlStack* stack) {
  if (m_handle != nullptr)
    throw torrent::internal_error("CurlGet::prepare_start(...) called on a busy object.");

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
CurlGet::activate() {
  CURLMcode code = curl_multi_add_handle(m_stack->handle(), m_handle);

  if (code != CURLM_OK)
    throw torrent::internal_error("CurlGet::activate() error calling curl_multi_add_handle: " + std::string(curl_multi_strerror(code)));

  // Normally libcurl should handle the timeout. But sometimes that doesn't
  // work right so we do a fallback timeout that just aborts the transfer.
  //
  // TODO: Verify this is still needed, as it was added to work around during early libcurl
  // versions.
  if (m_timeout != 0)
    torrent::this_thread::scheduler()->update_wait_for_ceil_seconds(&m_task_timeout, 1min + 1s*m_timeout);

  m_active = true;
}

void
CurlGet::cleanup() {
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
CurlGet::retry_ipv6() {
  CURL* nhandle = curl_easy_duphandle(m_handle);

  curl_easy_setopt(nhandle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
  curl_easy_cleanup(m_handle);

  m_handle = nhandle;
  m_ipv6 = true;
}

int64_t
CurlGet::size_done() {
  curl_off_t d = 0;
  curl_easy_getinfo(m_handle, CURLINFO_SIZE_DOWNLOAD_T, &d);

  return d;
}

int64_t
CurlGet::size_total() {
  curl_off_t d = 0;
  curl_easy_getinfo(m_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &d);

  return d;
}

void
CurlGet::receive_timeout() {
  // return m_stack->transfer_done(m_handle, "Timed out");
  throw internal_error("CurlGet::receive_timeout() called, however this was a hack to work around libcurl not handling timeouts correctly.");
}

// TODO: Verify slots are handled while CurlGet and CurlStack are unlocked.

void
CurlGet::trigger_done() {
  ::utils::slot_list_call(m_signal_done);

  // if (should_delete_stream) {
  //   delete m_stream;
  //   m_stream = NULL;
  // }
}

void
CurlGet::trigger_failed(const std::string& message) {
  ::utils::slot_list_call(m_signal_failed, message);

  // if (should_delete_stream) {
  //   delete m_stream;
  //   m_stream = NULL;
  // }
}

}
