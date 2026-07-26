#include "config.h"

#include "test/net/test_curl_get.h"

#include <memory>
#include <sstream>

#include "net/curl_get.h"
#include "net/curl_stack.h"
#include "torrent/exceptions.h"
#include "torrent/net/http_get.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_curl_get, "net");

void
test_curl_get::test_start_get_after_close() {
  torrent::net::CurlStack stack(m_main_thread.get());

  auto stream   = std::make_shared<std::stringstream>();
  auto curl_get = std::make_shared<torrent::net::CurlGet>("http://127.0.0.1:1/announce", stream);

  torrent::net::CurlGet::start(curl_get, &stack);
  torrent::net::CurlGet::close_and_cancel_callbacks(curl_get, nullptr);

  CPPUNIT_ASSERT_NO_THROW(stack.start_get(curl_get));
  CPPUNIT_ASSERT(!curl_get->is_stacked());

  stack.shutdown();
}

// Adding slots must not make the CurlGet hold a reference to itself, as that would keep its url and
// its response stream alive for the lifetime of the process.
void
test_curl_get::test_slots_do_not_retain_stream() {
  std::weak_ptr<std::stringstream> weak_stream;

  {
    auto stream = std::make_shared<std::stringstream>();
    weak_stream = stream;

    torrent::net::HttpGet http_get;
    http_get.reset("http://127.0.0.1:1/announce", stream);

    http_get.add_done_slot(m_main_thread.get(), []() {});
    http_get.add_failed_slot(m_main_thread.get(), [](const std::string&) {});

    CPPUNIT_ASSERT(!weak_stream.expired());
  }

  CPPUNIT_ASSERT(weak_stream.expired());
}
