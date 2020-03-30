#include "config.h"

#include "test_http.h"

#include <sstream>
#include "torrent/http.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(test_http, "torrent");

#define HTTP_SETUP()                                                    \
  bool http_destroyed = false;                                          \
  bool stream_destroyed = false;                                        \
                                                                        \
  TestHttp* test_http = new TestHttp(&http_destroyed);                  \
  torrent::Http* http = test_http;                                      \
  std::stringstream* http_stream = new StringStream(&stream_destroyed); \
                                                                        \
  int done_counter = 0;                                                 \
  int failed_counter = 0;                                               \
                                                                        \
  http->set_stream(http_stream);                                        \
  http->signal_done().push_back(std::bind(&increment_value, &done_counter)); \
  http->signal_failed().push_back(std::bind(&increment_value, &failed_counter));

class StringStream : public std::stringstream {
public:
  StringStream(bool *destroyed) : m_destroyed(destroyed) {}
  ~StringStream() { *m_destroyed = true; }
private:
  bool* m_destroyed;
};

class TestHttp : public torrent::Http {
public:
  static const int flag_active = 0x1;

  TestHttp(bool *destroyed = NULL) : m_flags(0), m_destroyed(destroyed) {}
  virtual ~TestHttp() { if (m_destroyed) *m_destroyed = true; }
  
  virtual void start() { m_flags |= flag_active; }
  virtual void close() { m_flags &= ~flag_active; }

  bool trigger_signal_done();
  bool trigger_signal_failed();

private:
  int m_flags;
  bool* m_destroyed;
};

bool
TestHttp::trigger_signal_done() {
  if (!(m_flags & flag_active))
    return false;

  m_flags &= ~flag_active;
  trigger_done();
  return true;
}

bool
TestHttp::trigger_signal_failed() {
  if (!(m_flags & flag_active))
    return false;

  m_flags &= ~flag_active;
  trigger_failed("We Fail.");
  return true;
}

TestHttp* create_test_http() { return new TestHttp; }

static void increment_value(int* value) { (*value)++; }

void
test_http::test_basic() {
  torrent::Http::slot_factory() = std::bind(&create_test_http);

  torrent::Http* http = torrent::Http::slot_factory()();
  std::stringstream* http_stream = new std::stringstream;

  http->set_url("http://example.com");
  CPPUNIT_ASSERT(http->url() == "http://example.com");

  CPPUNIT_ASSERT(http->stream() == NULL);
  http->set_stream(http_stream);
  CPPUNIT_ASSERT(http->stream() == http_stream);

  CPPUNIT_ASSERT(http->timeout() == 0);
  http->set_timeout(666);
  CPPUNIT_ASSERT(http->timeout() == 666);
  
  delete http;
  delete http_stream;
}

void
test_http::test_done() {
  HTTP_SETUP();
  http->start();

  CPPUNIT_ASSERT(test_http->trigger_signal_done());

  // Check that we didn't delete...

  CPPUNIT_ASSERT(done_counter == 1 && failed_counter == 0);
}

void
test_http::test_failure() {
  HTTP_SETUP();
  http->start();

  CPPUNIT_ASSERT(test_http->trigger_signal_failed());

  // Check that we didn't delete...

  CPPUNIT_ASSERT(done_counter == 0 && failed_counter == 1);
}

void
test_http::test_delete_on_done() {
  HTTP_SETUP();
  http->start();
  http->set_delete_stream();

  CPPUNIT_ASSERT(!stream_destroyed);
  CPPUNIT_ASSERT(!http_destroyed);
  CPPUNIT_ASSERT(test_http->trigger_signal_done());
  CPPUNIT_ASSERT(stream_destroyed);
  CPPUNIT_ASSERT(!http_destroyed);
  CPPUNIT_ASSERT(http->stream() == NULL);

  stream_destroyed = false;
  http_stream = new StringStream(&stream_destroyed);
  http->set_stream(http_stream);

  http->start();
  http->set_delete_self();

  CPPUNIT_ASSERT(!stream_destroyed);
  CPPUNIT_ASSERT(!http_destroyed);
  CPPUNIT_ASSERT(test_http->trigger_signal_done());
  CPPUNIT_ASSERT(stream_destroyed);
  CPPUNIT_ASSERT(http_destroyed);
}

void
test_http::test_delete_on_failure() {
  HTTP_SETUP();
  http->start();
  http->set_delete_stream();

  CPPUNIT_ASSERT(!stream_destroyed);
  CPPUNIT_ASSERT(!http_destroyed);
  CPPUNIT_ASSERT(test_http->trigger_signal_failed());
  CPPUNIT_ASSERT(stream_destroyed);
  CPPUNIT_ASSERT(!http_destroyed);
  CPPUNIT_ASSERT(http->stream() == NULL);

  stream_destroyed = false;
  http_stream = new StringStream(&stream_destroyed);
  http->set_stream(http_stream);

  http->start();
  http->set_delete_self();

  CPPUNIT_ASSERT(!stream_destroyed);
  CPPUNIT_ASSERT(!http_destroyed);
  CPPUNIT_ASSERT(test_http->trigger_signal_failed());
  CPPUNIT_ASSERT(stream_destroyed);
  CPPUNIT_ASSERT(http_destroyed);
}

