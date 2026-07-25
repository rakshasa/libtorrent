#include "config.h"

#include "test/net/test_curl_get.h"

#include <sstream>

#include "net/curl_get.h"
#include "net/curl_stack.h"
#include "torrent/exceptions.h"

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
