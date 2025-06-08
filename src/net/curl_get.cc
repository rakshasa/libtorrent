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

CurlGet::CurlGet(CurlStack* s)
  : m_stack(s) {

  m_task_timeout.slot() = [this]() { receive_timeout(); };
}

CurlGet::~CurlGet() {
  assert(!is_busy() && "CurlGet::~CurlGet called while still busy.");

  // TODO: We need to make it so that remove_get is called only from close and not dtor.
  //
  // Do we keep a vector of shared_ptr's of active CurlGet objects in stack?
  //
  // We can then check the use_count, if it is one during any point of the busy (in stack) lifetime, cancel the download.
  // close();
}

// TODO: Modify this to be called from CurlStack so we have the shared_ptr.

// TODO: When we add callback for start/close add an atomic_bool to indicate we've queued the
// action, and use that to tell the user that the http_get is busy or not.

void
CurlGet::prepare_start() {
  if (is_busy())
    throw torrent::internal_error("Tried to call CurlGet::start on a busy object.");

  if (m_stream == nullptr)
    throw torrent::internal_error("Tried to call CurlGet::start without a valid output stream.");

  // TODO: Remove this if we call from within CurlStack.
  // if (!m_stack->is_running())
  //   return; //////////////////

  m_handle = curl_easy_init();

  if (m_handle == NULL)
    throw torrent::internal_error("Call to curl_easy_init() failed.");

  curl_easy_setopt(m_handle, CURLOPT_URL,            m_url.c_str());
  curl_easy_setopt(m_handle, CURLOPT_WRITEFUNCTION,  &curl_get_receive_write);
  curl_easy_setopt(m_handle, CURLOPT_WRITEDATA,      this);

  if (m_timeout != 0) {
    curl_easy_setopt(m_handle, CURLOPT_CONNECTTIMEOUT, (long)60);
    curl_easy_setopt(m_handle, CURLOPT_TIMEOUT,        (long)m_timeout);

    // Normally libcurl should handle the timeout. But sometimes that doesn't
    // work right so we do a fallback timeout that just aborts the transfer.
    torrent::this_thread::scheduler()->update_wait_for_ceil_seconds(&m_task_timeout, 5s + 1s*m_timeout);
  }

  curl_easy_setopt(m_handle, CURLOPT_FORBID_REUSE,   (long)1);
  curl_easy_setopt(m_handle, CURLOPT_NOSIGNAL,       (long)1);
  curl_easy_setopt(m_handle, CURLOPT_FOLLOWLOCATION, (long)1);
  curl_easy_setopt(m_handle, CURLOPT_MAXREDIRS,      (long)5);

  curl_easy_setopt(m_handle, CURLOPT_IPRESOLVE,      CURL_IPRESOLVE_WHATEVER);
  curl_easy_setopt(m_handle, CURLOPT_ENCODING,       "");

  m_ipv6 = false;
}

void
CurlGet::cleanup() {
  if (!is_busy())
    throw torrent::internal_error("Tried to call CurlGet::close on a non-busy object.");

  torrent::this_thread::scheduler()->erase(&m_task_timeout);

  curl_easy_cleanup(m_handle);
  m_handle = nullptr;
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
  return m_stack->transfer_done(m_handle, "Timed out");
}

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
